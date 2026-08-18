#include <algorithm>
#include <array>
#include <limits>
#include <mwfl/ipc.h>
#include <sddl.h>
namespace mwfl {
namespace {
class FlagGuard final {
   public:
    explicit FlagGuard(std::atomic_flag& flag) : flag_(flag), acquired_(!flag_.test_and_set()) {}
    ~FlagGuard() {
        if (acquired_) flag_.clear();
    }
    bool Acquired() const { return acquired_; }

   private:
    std::atomic_flag& flag_;
    bool acquired_;
};
DWORD RemainingMilliseconds(Deadline deadline) {
    const auto remaining = deadline.Remaining();
    if (remaining.count() < 0) return INFINITE;
    return static_cast<DWORD>((std::min)(remaining.count(), static_cast<long long>(INFINITE - 1)));
}
bool Valid(const PipeEndpoint& options) {
    return options.name.starts_with(L"\\\\.\\pipe\\") && options.maximum_frame_size > 0 &&
           options.maximum_frame_size <= MaximumPipeFrameSize &&
           (options.access.kind != PipeAccessPolicyKind::ExplicitSids ||
            !options.access.allowed_sids.empty());
}
Result<std::wstring> CurrentUserSid() {
    HANDLE raw = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &raw))
        return SystemError::LastWin32().WithOperation(L"OpenProcessToken");
    KernelHandle token(raw);
    DWORD bytes = 0;
    GetTokenInformation(token.Get(), TokenUser, nullptr, 0, &bytes);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
        return SystemError::LastWin32().WithOperation(L"GetTokenInformation");
    std::vector<std::byte> storage(bytes);
    if (!GetTokenInformation(token.Get(), TokenUser, storage.data(), bytes, &bytes))
        return SystemError::LastWin32().WithOperation(L"GetTokenInformation");
    auto* user = reinterpret_cast<TOKEN_USER*>(storage.data());
    wchar_t* sid = nullptr;
    if (!ConvertSidToStringSidW(user->User.Sid, &sid))
        return SystemError::LastWin32().WithOperation(L"ConvertSidToStringSidW");
    std::wstring result(sid);
    LocalFree(sid);
    return result;
}
Result<std::wstring> CanonicalSid(std::wstring_view value) {
    PSID sid = nullptr;
    std::wstring copy(value);
    if (!ConvertStringSidToSidW(copy.c_str(), &sid))
        return SystemError::LastWin32().WithOperation(L"ConvertStringSidToSidW");
    wchar_t* canonical = nullptr;
    if (!ConvertSidToStringSidW(sid, &canonical)) {
        LocalFree(sid);
        return SystemError::LastWin32().WithOperation(L"ConvertSidToStringSidW");
    }
    std::wstring result(canonical);
    LocalFree(canonical);
    LocalFree(sid);
    return result;
}
Result<KernelHandle> CreateListener(const PipeEndpoint& options) {
    auto user = CurrentUserSid();
    if (!user) return user.GetError();
    std::vector<std::wstring> sids;
    if (options.access.kind != PipeAccessPolicyKind::ExplicitSids) sids.push_back(user.Value());
    if (options.access.kind == PipeAccessPolicyKind::CurrentUserAndLocalSystem)
        sids.push_back(L"S-1-5-18");
    if (options.access.kind == PipeAccessPolicyKind::ExplicitSids) {
        for (const auto& value : options.access.allowed_sids) {
            auto sid = CanonicalSid(value);
            if (!sid) return sid.GetError();
            sids.push_back(std::move(sid.Value()));
        }
    }
    std::wstring sddl = L"D:P";
    for (const auto& sid : sids) sddl += L"(A;;GA;;;" + sid + L")";
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(), SDDL_REVISION_1,
                                                              &descriptor, nullptr))
        return SystemError::LastWin32().WithOperation(L"Pipe security descriptor");
    SECURITY_ATTRIBUTES sa{sizeof(sa), descriptor, FALSE};
    HANDLE raw = CreateNamedPipeW(
        options.name.c_str(), PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
        PIPE_UNLIMITED_INSTANCES, options.maximum_frame_size + 4, options.maximum_frame_size + 4, 0,
        &sa);
    LocalFree(descriptor);
    if (raw == INVALID_HANDLE_VALUE)
        return SystemError::LastWin32().WithOperation(L"CreateNamedPipeW");
    return KernelHandle(raw);
}
Result<CompletionStatus> AwaitOverlapped(HANDLE pipe, OVERLAPPED& operation, Deadline deadline,
                                         std::stop_token stop) {
    if (stop.stop_requested()) {
        CancelIoEx(pipe, &operation);
        return CompletionStatus::Cancelled;
    }
    KernelHandle cancelled(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    if (!cancelled) return SystemError::LastWin32();
    std::stop_callback callback(stop, [h = cancelled.Get()] { SetEvent(h); });
    std::array<HANDLE, 2> handles{operation.hEvent, cancelled.Get()};
    const DWORD waited =
        WaitForMultipleObjects(2, handles.data(), FALSE, RemainingMilliseconds(deadline));
    if (waited == WAIT_OBJECT_0) return CompletionStatus::Completed;
    CancelIoEx(pipe, &operation);
    DWORD ignored = 0;
    GetOverlappedResult(pipe, &operation, &ignored, TRUE);
    if (waited == WAIT_OBJECT_0 + 1) return CompletionStatus::Cancelled;
    if (waited == WAIT_TIMEOUT) return CompletionStatus::TimedOut;
    return SystemError::LastWin32().WithOperation(L"Wait for pipe I/O");
}
Result<CompletionStatus> Transfer(HANDLE pipe, void* data, std::uint32_t size, bool write,
                                  Deadline deadline, std::stop_token stop) {
    auto* cursor = static_cast<std::byte*>(data);
    std::uint32_t remaining = size;
    while (remaining) {
        KernelHandle event(CreateEventW(nullptr, TRUE, FALSE, nullptr));
        if (!event) return SystemError::LastWin32();
        OVERLAPPED operation{};
        operation.hEvent = event.Get();
        DWORD transferred = 0;
        const BOOL started = write ? WriteFile(pipe, cursor, remaining, nullptr, &operation)
                                   : ReadFile(pipe, cursor, remaining, nullptr, &operation);
        if (!started && GetLastError() != ERROR_IO_PENDING) {
            const DWORD error = GetLastError();
            if (error == ERROR_BROKEN_PIPE || error == ERROR_PIPE_NOT_CONNECTED ||
                error == ERROR_NO_DATA)
                return CompletionStatus::Disconnected;
            return SystemError::FromWin32(error).WithOperation(write ? L"WriteFile pipe"
                                                                     : L"ReadFile pipe");
        }
        auto awaited = AwaitOverlapped(pipe, operation, deadline, stop);
        if (!awaited || awaited.Value() != CompletionStatus::Completed) return awaited;
        if (!GetOverlappedResult(pipe, &operation, &transferred, FALSE)) {
            const DWORD error = GetLastError();
            if (error == ERROR_BROKEN_PIPE || error == ERROR_PIPE_NOT_CONNECTED ||
                error == ERROR_OPERATION_ABORTED)
                return error == ERROR_OPERATION_ABORTED && stop.stop_requested()
                           ? CompletionStatus::Cancelled
                           : CompletionStatus::Disconnected;
            return SystemError::FromWin32(error);
        }
        if (!transferred) return CompletionStatus::Disconnected;
        cursor += transferred;
        remaining -= transferred;
    }
    return CompletionStatus::Completed;
}
Result<PipePeer> IdentityForProcess(DWORD process_id) {
    KernelHandle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process_id));
    if (!process) return SystemError::LastWin32().WithOperation(L"Open peer process");
    HANDLE raw_token = nullptr;
    if (!OpenProcessToken(process.Get(), TOKEN_QUERY, &raw_token))
        return SystemError::LastWin32().WithOperation(L"Open peer token");
    KernelHandle token(raw_token);
    DWORD bytes = 0;
    GetTokenInformation(token.Get(), TokenUser, nullptr, 0, &bytes);
    std::vector<std::byte> storage(bytes);
    if (!GetTokenInformation(token.Get(), TokenUser, storage.data(), bytes, &bytes))
        return SystemError::LastWin32();
    auto* user = reinterpret_cast<TOKEN_USER*>(storage.data());
    wchar_t* sid = nullptr;
    if (!ConvertSidToStringSidW(user->User.Sid, &sid)) return SystemError::LastWin32();
    std::wstring text(sid);
    LocalFree(sid);
    DWORD session = 0;
    if (!ProcessIdToSessionId(process_id, &session)) return SystemError::LastWin32();
    return PipePeer{process_id, session, std::move(text)};
}
}  // namespace
PipeConnection::PipeConnection(KernelHandle pipe, std::uint32_t maximum, bool server) noexcept
    : pipe_(std::move(pipe)), maximum_frame_size_(maximum), server_end_(server) {}
PipeConnection::PipeConnection(PipeConnection&& other) noexcept
    : pipe_(std::move(other.pipe_)),
      maximum_frame_size_(other.maximum_frame_size_),
      server_end_(other.server_end_) {}
PipeConnection& PipeConnection::operator=(PipeConnection&& other) noexcept {
    if (this != &other) {
        pipe_ = std::move(other.pipe_);
        maximum_frame_size_ = other.maximum_frame_size_;
        server_end_ = other.server_end_;
        reading_.clear();
        writing_.clear();
    }
    return *this;
}
Result<OperationOutcome<void>> PipeConnection::WriteFrame(std::span<const std::byte> payload,
                                                          Deadline deadline,
                                                          std::stop_token stop) {
    FlagGuard guard(writing_);
    if (!guard.Acquired())
        return SystemError::FromWin32(ERROR_BUSY).WithOperation(L"Concurrent pipe write");
    if (!pipe_ || payload.size() > maximum_frame_size_ || payload.size() > MaximumPipeFrameSize)
        return SystemError::FromWin32(ERROR_INVALID_DATA);
    const std::uint32_t size = static_cast<std::uint32_t>(payload.size());
    std::array<std::byte, 4> header{std::byte(size & 0xff), std::byte((size >> 8) & 0xff),
                                    std::byte((size >> 16) & 0xff), std::byte((size >> 24) & 0xff)};
    auto sent = Transfer(pipe_.Get(), header.data(), 4, true, deadline, stop);
    if (!sent) return sent.GetError();
    if (sent.Value() != CompletionStatus::Completed)
        return OperationOutcome<void>::Control(sent.Value());
    if (payload.empty()) return OperationOutcome<void>::Completed();
    auto body =
        Transfer(pipe_.Get(), const_cast<std::byte*>(payload.data()), size, true, deadline, stop);
    if (!body) return body.GetError();
    return body.Value() == CompletionStatus::Completed
               ? OperationOutcome<void>::Completed()
               : OperationOutcome<void>::Control(body.Value());
}
Result<OperationOutcome<std::vector<std::byte>>> PipeConnection::ReadFrame(
    Deadline deadline, std::stop_token stop) {
    FlagGuard guard(reading_);
    if (!guard.Acquired())
        return SystemError::FromWin32(ERROR_BUSY).WithOperation(L"Concurrent pipe read");
    if (!pipe_) return SystemError::FromWin32(ERROR_INVALID_HANDLE);
    std::array<std::byte, 4> header{};
    auto read = Transfer(pipe_.Get(), header.data(), 4, false, deadline, stop);
    if (!read) return read.GetError();
    if (read.Value() != CompletionStatus::Completed)
        return OperationOutcome<std::vector<std::byte>>::Control(read.Value());
    const auto size = std::to_integer<std::uint32_t>(header[0]) |
                      (std::to_integer<std::uint32_t>(header[1]) << 8) |
                      (std::to_integer<std::uint32_t>(header[2]) << 16) |
                      (std::to_integer<std::uint32_t>(header[3]) << 24);
    if (size > maximum_frame_size_ || size > MaximumPipeFrameSize)
        return SystemError::FromWin32(ERROR_BUFFER_OVERFLOW).WithOperation(L"Pipe frame length");
    std::vector<std::byte> payload(size);
    if (!size) return OperationOutcome<std::vector<std::byte>>::Completed({});
    auto body = Transfer(pipe_.Get(), payload.data(), size, false, deadline, stop);
    if (!body) return body.GetError();
    if (body.Value() != CompletionStatus::Completed)
        return OperationOutcome<std::vector<std::byte>>::Control(body.Value());
    return OperationOutcome<std::vector<std::byte>>::Completed(std::move(payload));
}
Result<PipePeer> PipeConnection::QueryPeer() const {
    if (!pipe_) return SystemError::FromWin32(ERROR_INVALID_HANDLE);
    ULONG id = 0;
    const BOOL ok = server_end_ ? GetNamedPipeClientProcessId(pipe_.Get(), &id)
                                : GetNamedPipeServerProcessId(pipe_.Get(), &id);
    if (!ok) return SystemError::LastWin32().WithOperation(L"Query pipe peer process");
    return IdentityForProcess(id);
}
PipeChannels PipeConnection::Split() && {
    auto shared = std::make_shared<PipeConnection>(std::move(*this));
    return {PipeReader(shared), PipeWriter(std::move(shared))};
}
Result<OperationOutcome<std::vector<std::byte>>> PipeReader::ReadFrame(
    Deadline deadline, std::stop_token stop) {
    if (!connection_) return SystemError::FromWin32(ERROR_INVALID_HANDLE);
    return connection_->ReadFrame(deadline, stop);
}
Result<OperationOutcome<void>> PipeWriter::WriteFrame(std::span<const std::byte> payload,
                                                      Deadline deadline,
                                                      std::stop_token stop) {
    if (!connection_) return SystemError::FromWin32(ERROR_INVALID_HANDLE);
    return connection_->WriteFrame(payload, deadline, stop);
}
Result<PipeListener> PipeListener::Create(PipeEndpoint options) {
    if (!Valid(options)) return SystemError::FromWin32(ERROR_INVALID_PARAMETER);
    auto listener = CreateListener(options);
    if (!listener) return listener.GetError();
    return PipeListener(std::move(options), std::move(listener.Value()));
}
Result<OperationOutcome<PipeConnection>> PipeListener::Accept(Deadline deadline,
                                                               std::stop_token stop) {
    if (!listener_) return SystemError::FromWin32(ERROR_INVALID_STATE);
    KernelHandle event(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    if (!event) return SystemError::LastWin32();
    OVERLAPPED operation{};
    operation.hEvent = event.Get();
    const BOOL connected = ConnectNamedPipe(listener_.Get(), &operation);
    if (!connected) {
        const DWORD error = GetLastError();
        if (error == ERROR_PIPE_CONNECTED)
            SetEvent(event.Get());
        else if (error != ERROR_IO_PENDING)
            return SystemError::FromWin32(error).WithOperation(L"ConnectNamedPipe");
    }
    auto awaited = AwaitOverlapped(listener_.Get(), operation, deadline, stop);
    if (!awaited) return awaited.GetError();
    if (awaited.Value() != CompletionStatus::Completed)
        return OperationOutcome<PipeConnection>::Control(awaited.Value());

    // Arm the next instance before handing the connected instance to the caller. This keeps
    // PipeListener reusable and prevents reconnects from depending on application retry sleeps.
    auto next_listener = CreateListener(endpoint_);
    if (!next_listener) return next_listener.GetError().WithOperation(L"Create next pipe listener");
    PipeConnection connection(std::move(listener_), endpoint_.maximum_frame_size, true);
    listener_ = std::move(next_listener.Value());
    return OperationOutcome<PipeConnection>::Completed(std::move(connection));
}
Result<OperationOutcome<PipeConnection>> ConnectPipe(const PipeEndpoint& endpoint,
                                                      Deadline deadline,
                                                      std::stop_token stop) {
    if (!Valid(endpoint)) return SystemError::FromWin32(ERROR_INVALID_PARAMETER);
    while (true) {
        if (stop.stop_requested())
            return OperationOutcome<PipeConnection>::Control(CompletionStatus::Cancelled);
        HANDLE raw = CreateFileW(endpoint.name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                                 OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
        if (raw != INVALID_HANDLE_VALUE)
            return OperationOutcome<PipeConnection>::Completed(
                PipeConnection(KernelHandle(raw), endpoint.maximum_frame_size, false));
        const DWORD error = GetLastError();
        if (error != ERROR_PIPE_BUSY && error != ERROR_FILE_NOT_FOUND)
            return SystemError::FromWin32(error).WithOperation(L"CreateFileW pipe");
        const DWORD remaining = RemainingMilliseconds(deadline);
        if (!remaining)
            return OperationOutcome<PipeConnection>::Control(CompletionStatus::TimedOut);
        Sleep((std::min<DWORD>)(remaining, 10));
    }
}
ScopedPipeImpersonation::~ScopedPipeImpersonation() {
    if (active_) RevertToSelf();
}
ScopedPipeImpersonation::ScopedPipeImpersonation(ScopedPipeImpersonation&& other) noexcept
    : active_(std::exchange(other.active_, false)) {}
ScopedPipeImpersonation& ScopedPipeImpersonation::operator=(
    ScopedPipeImpersonation&& other) noexcept {
    if (this != &other) {
        if (active_) RevertToSelf();
        active_ = std::exchange(other.active_, false);
    }
    return *this;
}
Result<ScopedPipeImpersonation> ScopedPipeImpersonation::Create(PipeConnection& connection) {
    if (!connection.server_end_ || !connection.pipe_)
        return SystemError::FromWin32(ERROR_INVALID_STATE);
    if (!ImpersonateNamedPipeClient(connection.pipe_.Get()))
        return SystemError::LastWin32().WithOperation(L"ImpersonateNamedPipeClient");
    return ScopedPipeImpersonation(true);
}
}  // namespace mwfl
