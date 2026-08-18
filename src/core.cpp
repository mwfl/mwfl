#include <algorithm>
#include <array>
#include <limits>
#include <mwfl/core.h>
namespace mwfl {
namespace {
std::wstring FormatSystemMessage(DWORD code) {
    wchar_t* buffer = nullptr;
    const DWORD length = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, code, 0, reinterpret_cast<wchar_t*>(&buffer), 0, nullptr);
    if (length == 0 || buffer == nullptr) return L"Windows error " + std::to_wstring(code);
    std::wstring message(buffer, length);
    LocalFree(buffer);
    while (!message.empty() &&
           (message.back() == L'\r' || message.back() == L'\n' || message.back() == L' '))
        message.pop_back();
    return message;
}
DWORD BoundedTimeout(Deadline deadline) noexcept {
    const auto remaining = deadline.Remaining();
    if (remaining.count() < 0) return INFINITE;
    return static_cast<DWORD>((std::min)(remaining.count(), static_cast<long long>(INFINITE - 1)));
}
}  // namespace
SystemError SystemError::FromWin32(DWORD value) noexcept {
    return {ErrorDomain::Win32, value == ERROR_SUCCESS ? ERROR_GEN_FAILURE : value};
}
SystemError SystemError::LastWin32() noexcept {
    return FromWin32(GetLastError());
}
SystemError SystemError::FromHResult(HRESULT value) noexcept {
    return {ErrorDomain::HResult, static_cast<std::uint32_t>(value)};
}
SystemError SystemError::Application(std::uint32_t code, std::wstring detail) {
    return {ErrorDomain::Application, code, {}, std::move(detail)};
}
SystemError SystemError::Protocol(std::uint32_t code, std::wstring detail) {
    return {ErrorDomain::Protocol, code, {}, std::move(detail)};
}
SystemError SystemError::Policy(std::uint32_t code, std::wstring detail) {
    return {ErrorDomain::Policy, code, {}, std::move(detail)};
}
SystemError SystemError::InvalidUsage(std::uint32_t code, std::wstring detail) {
    return {ErrorDomain::InvalidUsage, code, {}, std::move(detail)};
}
SystemError SystemError::WithOperation(std::wstring value) const {
    SystemError copy = *this;
    copy.operation = std::move(value);
    return copy;
}
std::wstring SystemError::Message() const {
    std::wstring message = domain == ErrorDomain::Win32 || domain == ErrorDomain::HResult
                               ? FormatSystemMessage(code)
                               : L"MWFL error " + std::to_wstring(code);
    if (!detail.empty()) message += L": " + detail;
    return operation.empty() ? message : operation + L": " + message;
}
Deadline Deadline::After(std::chrono::milliseconds duration) noexcept {
    if (duration.count() < 0) return Infinite();
    return Deadline{std::chrono::steady_clock::now() + duration, false};
}
Deadline Deadline::Infinite() noexcept { return Deadline{{}, true}; }
bool Deadline::Expired() const noexcept {
    return !infinite_ && std::chrono::steady_clock::now() >= end_;
}
std::chrono::milliseconds Deadline::Remaining() const noexcept {
    if (infinite_) return std::chrono::milliseconds{-1};
    const auto now = std::chrono::steady_clock::now();
    if (now >= end_) return std::chrono::milliseconds{0};
    return std::chrono::ceil<std::chrono::milliseconds>(end_ - now);
}
Result<OperationOutcome<WaitObservation>> WaitForHandle(HANDLE handle, Deadline deadline,
                                                         std::stop_token stop) {
    if (!KernelHandlePolicy::IsValid(handle)) return SystemError::FromWin32(ERROR_INVALID_HANDLE);
    if (stop.stop_requested())
        return OperationOutcome<WaitObservation>::Control(CompletionStatus::Cancelled);
    KernelHandle cancelled(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    if (!cancelled) return SystemError::LastWin32();
    std::stop_callback callback(stop, [event = cancelled.Get()] { SetEvent(event); });
    const std::array<HANDLE, 2> handles{handle, cancelled.Get()};
    const DWORD result = WaitForMultipleObjects(static_cast<DWORD>(handles.size()), handles.data(),
                                                FALSE, BoundedTimeout(deadline));
    // The observed handle order is the tie breaker: completion wins when the
    // native handle and cancellation event become signaled together.
    if (result == WAIT_OBJECT_0)
        return OperationOutcome<WaitObservation>::Completed(WaitObservation::Signaled);
    if (result == WAIT_OBJECT_0 + 1)
        return OperationOutcome<WaitObservation>::Control(CompletionStatus::Cancelled);
    if (result == WAIT_TIMEOUT)
        return OperationOutcome<WaitObservation>::Control(CompletionStatus::TimedOut);
    if (result >= WAIT_ABANDONED_0 && result < WAIT_ABANDONED_0 + handles.size())
        return OperationOutcome<WaitObservation>::Completed(WaitObservation::Abandoned);
    return SystemError::LastWin32();
}
Result<std::wstring> Utf8ToWide(std::string_view text) {
    if (text.empty()) return std::wstring{};
    if (text.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)()))
        return SystemError::FromWin32(ERROR_INVALID_PARAMETER);
    const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                                         static_cast<int>(text.size()), nullptr, 0);
    if (size == 0) return SystemError::LastWin32();
    std::wstring converted(static_cast<std::size_t>(size), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                            static_cast<int>(text.size()), converted.data(), size) == 0)
        return SystemError::LastWin32();
    return converted;
}
Result<std::string> WideToUtf8(std::wstring_view text) {
    if (text.empty()) return std::string{};
    if (text.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)()))
        return SystemError::FromWin32(ERROR_INVALID_PARAMETER);
    const int size =
        WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(),
                            static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    if (size == 0) return SystemError::LastWin32();
    std::string converted(static_cast<std::size_t>(size), '\0');
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(),
                            static_cast<int>(text.size()), converted.data(), size, nullptr,
                            nullptr) == 0)
        return SystemError::LastWin32();
    return converted;
}
}  // namespace mwfl
