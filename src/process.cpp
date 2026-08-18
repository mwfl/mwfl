#include <algorithm>
#include <array>
#include <map>
#include <mwfl/process.h>
#include <thread>
namespace mwfl {
namespace {
struct CaseInsensitiveLess {
    bool operator()(const std::wstring& a, const std::wstring& b) const noexcept {
        return _wcsicmp(a.c_str(), b.c_str()) < 0;
    }
};
Result<KernelHandle> CreateJob(const ProcessJobOptions& options) {
    KernelHandle job(CreateJobObjectW(nullptr, nullptr));
    if (!job) return SystemError::LastWin32().WithOperation(L"CreateJobObjectW");
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    if (options.kill_on_close)
        limits.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (options.active_process_limit) {
        limits.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_ACTIVE_PROCESS;
        limits.BasicLimitInformation.ActiveProcessLimit = *options.active_process_limit;
    }
    if (options.process_memory_limit) {
        limits.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_PROCESS_MEMORY;
        limits.ProcessMemoryLimit = *options.process_memory_limit;
    }
    if (options.job_memory_limit) {
        limits.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_JOB_MEMORY;
        limits.JobMemoryLimit = *options.job_memory_limit;
    }
    if (!SetInformationJobObject(job.Get(), JobObjectExtendedLimitInformation, &limits,
                                 sizeof(limits)))
        return SystemError::LastWin32().WithOperation(L"SetInformationJobObject");
    if (options.cpu_rate_hard_cap) {
        if (*options.cpu_rate_hard_cap < 1 || *options.cpu_rate_hard_cap > 10000)
            return SystemError::InvalidUsage(ERROR_INVALID_PARAMETER, L"CPU rate must be 1..10000")
                .WithOperation(L"Create process job");
        JOBOBJECT_CPU_RATE_CONTROL_INFORMATION cpu{};
        cpu.ControlFlags = JOB_OBJECT_CPU_RATE_CONTROL_ENABLE | JOB_OBJECT_CPU_RATE_CONTROL_HARD_CAP;
        cpu.CpuRate = *options.cpu_rate_hard_cap;
        if (!SetInformationJobObject(job.Get(), JobObjectCpuRateControlInformation, &cpu,
                                     sizeof(cpu)))
            return SystemError::LastWin32().WithOperation(L"Set job CPU rate");
    }
    return std::move(job);
}
Result<std::pair<KernelHandle, KernelHandle>> CreateOutputPipe() {
    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
    HANDLE read = nullptr, write = nullptr;
    if (!CreatePipe(&read, &write, &sa, 0))
        return SystemError::LastWin32().WithOperation(L"CreatePipe");
    KernelHandle r(read), w(write);
    if (!SetHandleInformation(r.Get(), HANDLE_FLAG_INHERIT, 0))
        return SystemError::LastWin32().WithOperation(L"SetHandleInformation");
    return std::pair<KernelHandle, KernelHandle>{std::move(r), std::move(w)};
}
Result<std::pair<KernelHandle, KernelHandle>> CreateInputPipe() {
    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
    HANDLE read = nullptr, write = nullptr;
    if (!CreatePipe(&read, &write, &sa, 0))
        return SystemError::LastWin32().WithOperation(L"Create stdin pipe");
    KernelHandle r(read), w(write);
    if (!SetHandleInformation(w.Get(), HANDLE_FLAG_INHERIT, 0))
        return SystemError::LastWin32().WithOperation(L"Set stdin pipe inheritance");
    return std::pair<KernelHandle, KernelHandle>{std::move(r), std::move(w)};
}
Result<KernelHandle> DuplicateInheritable(HANDLE source, bool input, std::wstring_view operation) {
    if (!source || source == INVALID_HANDLE_VALUE) {
        SECURITY_ATTRIBUTES attributes{sizeof(attributes), nullptr, TRUE};
        HANDLE null_handle = CreateFileW(L"NUL", input ? GENERIC_READ : GENERIC_WRITE,
                                         FILE_SHARE_READ | FILE_SHARE_WRITE, &attributes,
                                         OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (null_handle == INVALID_HANDLE_VALUE)
            return SystemError::LastWin32().WithOperation(std::wstring(operation));
        return KernelHandle(null_handle);
    }
    HANDLE duplicate = nullptr;
    if (!DuplicateHandle(GetCurrentProcess(), source, GetCurrentProcess(), &duplicate, 0, TRUE,
                         DUPLICATE_SAME_ACCESS))
        return SystemError::LastWin32().WithOperation(std::wstring(operation));
    return KernelHandle(duplicate);
}
Result<std::vector<wchar_t>> BuildEnvironment(
    bool inherit,
    const std::vector<std::pair<std::wstring, std::optional<std::wstring>>>& changes) {
    std::map<std::wstring, std::wstring, CaseInsensitiveLess> values;
    if (inherit) {
        wchar_t* block = GetEnvironmentStringsW();
        if (!block) return SystemError::LastWin32().WithOperation(L"GetEnvironmentStringsW");
        for (const wchar_t* p = block; *p; p += wcslen(p) + 1) {
            std::wstring_view entry(p);
            const auto eq = entry.find(L'=', entry.starts_with(L'=') ? 1 : 0);
            if (eq != std::wstring_view::npos)
                values.emplace(std::wstring(entry.substr(0, eq)),
                               std::wstring(entry.substr(eq + 1)));
        }
        FreeEnvironmentStringsW(block);
    }
    for (const auto& [name, value] : changes) {
        if (name.empty() || name.find(L'=') != std::wstring::npos)
            return SystemError::FromWin32(ERROR_INVALID_PARAMETER)
                .WithOperation(L"Process environment");
        if (value)
            values[name] = *value;
        else
            values.erase(name);
    }
    std::vector<wchar_t> block;
    for (const auto& [name, value] : values) {
        block.insert(block.end(), name.begin(), name.end());
        block.push_back(L'=');
        block.insert(block.end(), value.begin(), value.end());
        block.push_back(L'\0');
    }
    block.push_back(L'\0');
    return block;
}
void DrainPipe(HANDLE pipe, std::size_t maximum, std::vector<std::byte>& output, bool& truncated,
               std::stop_token stop) {
    std::array<std::byte, 4096> buffer{};
    while (!stop.stop_requested()) {
        DWORD available = 0;
        if (!PeekNamedPipe(pipe, nullptr, 0, nullptr, &available, nullptr)) break;
        if (!available) {
            Sleep(2);
            continue;
        }
        DWORD read = 0;
        if (!ReadFile(pipe, buffer.data(), (std::min)(available, static_cast<DWORD>(buffer.size())),
                      &read, nullptr) ||
            !read)
            break;
        const auto keep = (std::min)(static_cast<std::size_t>(read),
                                     maximum > output.size() ? maximum - output.size() : 0);
        output.insert(output.end(), buffer.begin(),
                      buffer.begin() + static_cast<std::ptrdiff_t>(keep));
        truncated = truncated || keep != read;
    }
}
}  // namespace
std::wstring QuoteWindowsArgument(std::wstring_view argument) {
    if (!argument.empty() && argument.find_first_of(L" \t\"") == std::wstring_view::npos)
        return std::wstring(argument);
    std::wstring quoted(1, L'"');
    std::size_t slashes = 0;
    for (wchar_t c : argument) {
        if (c == L'\\') {
            ++slashes;
            continue;
        }
        if (c == L'"') {
            quoted.append(slashes * 2 + 1, L'\\');
            quoted.push_back(L'"');
            slashes = 0;
            continue;
        }
        quoted.append(slashes, L'\\');
        slashes = 0;
        quoted.push_back(c);
    }
    quoted.append(slashes * 2, L'\\');
    quoted.push_back(L'"');
    return quoted;
}
Process::Process(KernelHandle process, KernelHandle thread, KernelHandle job, KernelHandle input,
                 KernelHandle out, KernelHandle err, DWORD id, bool group) noexcept
    : process_(std::move(process)),
      thread_(std::move(thread)),
      job_(std::move(job)),
      stdin_write_(std::move(input)),
      stdout_read_(std::move(out)),
      stderr_read_(std::move(err)),
      id_(id),
      new_process_group_(group) {}
Result<OperationOutcome<DWORD>> Process::Wait(Deadline deadline, std::stop_token stop) const {
    auto waited = WaitForHandle(process_.Get(), deadline, stop);
    if (!waited) return waited.GetError().WithOperation(L"Wait for process");
    if (waited.Value().status != CompletionStatus::Completed)
        return OperationOutcome<DWORD>::Control(waited.Value().status);
    DWORD code = 0;
    if (!GetExitCodeProcess(process_.Get(), &code))
        return SystemError::LastWin32().WithOperation(L"GetExitCodeProcess");
    return OperationOutcome<DWORD>::Completed(code);
}
Result<void> Process::RequestConsoleStop() const noexcept {
    if (!new_process_group_ || !id_)
        return SystemError::FromWin32(ERROR_INVALID_STATE).WithOperation(L"RequestConsoleStop");
    if (!GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, id_))
        return SystemError::LastWin32().WithOperation(L"GenerateConsoleCtrlEvent");
    return {};
}
Result<void> Process::TerminateTree(DWORD code) noexcept {
    if (!job_) return SystemError::FromWin32(ERROR_INVALID_STATE).WithOperation(L"TerminateTree");
    if (!TerminateJobObject(job_.Get(), code))
        return SystemError::LastWin32().WithOperation(L"TerminateJobObject");
    return {};
}
Result<OperationOutcome<ProcessOutput>> Process::CollectOutput(std::size_t max_out,
                                                                std::size_t max_err,
                                                                Deadline deadline,
                                                                std::stop_token stop) {
    ProcessOutput output;
    std::stop_source drains;
    std::jthread out_thread, err_thread;
    if (stdout_read_)
        out_thread = std::jthread([&] {
            DrainPipe(stdout_read_.Get(), max_out, output.stdout_bytes, output.stdout_truncated,
                      drains.get_token());
        });
    if (stderr_read_)
        err_thread = std::jthread([&] {
            DrainPipe(stderr_read_.Get(), max_err, output.stderr_bytes, output.stderr_truncated,
                      drains.get_token());
        });
    auto waited = Wait(deadline, stop);
    if (!waited) {
        drains.request_stop();
        return waited.GetError();
    }
    if (waited.Value().status == CompletionStatus::Completed) {
        if (out_thread.joinable()) out_thread.join();
        if (err_thread.joinable()) err_thread.join();
        output.exit_code = *waited.Value().value;
    } else {
        drains.request_stop();
        return OperationOutcome<ProcessOutput>::Control(waited.Value().status);
    }
    return OperationOutcome<ProcessOutput>::Completed(std::move(output));
}
Result<OperationOutcome<std::size_t>> Process::WriteInput(std::span<const std::byte> input,
                                                          Deadline deadline,
                                                          std::stop_token stop) {
    if (!stdin_write_) return SystemError::FromWin32(ERROR_INVALID_HANDLE);
    if (input.empty()) return OperationOutcome<std::size_t>::Completed(0);
    if (input.size() > MAXDWORD) return SystemError::FromWin32(ERROR_FILE_TOO_LARGE);
    DWORD written = 0;
    DWORD error = ERROR_SUCCESS;
    std::jthread writer([&] {
        const auto size = static_cast<DWORD>(input.size());
        if (!WriteFile(stdin_write_.Get(), input.data(), size, &written, nullptr))
            error = GetLastError();
    });
    auto waited = WaitForHandle(writer.native_handle(), deadline, stop);
    if (!waited) {
        CancelSynchronousIo(writer.native_handle());
        writer.join();
        return waited.GetError();
    }
    if (waited.Value().status != CompletionStatus::Completed) {
        CancelSynchronousIo(writer.native_handle());
        writer.join();
        return OperationOutcome<std::size_t>::Control(waited.Value().status);
    }
    writer.join();
    if (error == ERROR_BROKEN_PIPE || error == ERROR_NO_DATA)
        return OperationOutcome<std::size_t>::Control(CompletionStatus::Disconnected);
    if (error != ERROR_SUCCESS)
        return SystemError::FromWin32(error).WithOperation(L"Write process stdin");
    return OperationOutcome<std::size_t>::Completed(written);
}
namespace {
Result<OperationOutcome<std::size_t>> ReadProcessPipe(HANDLE pipe, std::span<std::byte> output,
                                                       Deadline deadline,
                                                       std::stop_token stop,
                                                       std::wstring_view operation) {
    if (!pipe) return SystemError::FromWin32(ERROR_INVALID_HANDLE).WithOperation(std::wstring(operation));
    if (output.empty()) return OperationOutcome<std::size_t>::Completed(0);
    if (output.size() > MAXDWORD) return SystemError::FromWin32(ERROR_FILE_TOO_LARGE);
    DWORD read = 0;
    DWORD error = ERROR_SUCCESS;
    std::jthread reader([&] {
        if (!ReadFile(pipe, output.data(), static_cast<DWORD>(output.size()), &read, nullptr))
            error = GetLastError();
    });
    auto waited = WaitForHandle(reader.native_handle(), deadline, stop);
    if (!waited) {
        CancelSynchronousIo(reader.native_handle());
        reader.join();
        return waited.GetError();
    }
    if (waited.Value().status != CompletionStatus::Completed) {
        CancelSynchronousIo(reader.native_handle());
        reader.join();
        return OperationOutcome<std::size_t>::Control(waited.Value().status);
    }
    reader.join();
    if (error == ERROR_BROKEN_PIPE || error == ERROR_NO_DATA ||
        (error == ERROR_SUCCESS && read == 0))
        return OperationOutcome<std::size_t>::Control(CompletionStatus::Disconnected);
    if (error != ERROR_SUCCESS)
        return SystemError::FromWin32(error).WithOperation(std::wstring(operation));
    return OperationOutcome<std::size_t>::Completed(read);
}
}  // namespace
Result<OperationOutcome<std::size_t>> Process::ReadStdout(std::span<std::byte> output,
                                                          Deadline deadline,
                                                          std::stop_token stop) {
    return ReadProcessPipe(stdout_read_.Get(), output, deadline, stop, L"Read process stdout");
}
Result<OperationOutcome<std::size_t>> Process::ReadStderr(std::span<std::byte> output,
                                                          Deadline deadline,
                                                          std::stop_token stop) {
    return ReadProcessPipe(stderr_read_.Get(), output, deadline, stop, L"Read process stderr");
}
ProcessBuilder& ProcessBuilder::Executable(std::filesystem::path v) {
    executable_ = std::move(v);
    return *this;
}
ProcessBuilder& ProcessBuilder::Argument(std::wstring v) {
    arguments_.push_back(std::move(v));
    return *this;
}
ProcessBuilder& ProcessBuilder::WorkingDirectory(std::filesystem::path v) {
    working_directory_ = std::move(v);
    return *this;
}
ProcessBuilder& ProcessBuilder::InheritEnvironment(bool v) noexcept {
    inherit_environment_ = v;
    return *this;
}
ProcessBuilder& ProcessBuilder::Environment(std::wstring n, std::wstring v) {
    environment_.emplace_back(std::move(n), std::move(v));
    return *this;
}
ProcessBuilder& ProcessBuilder::RemoveEnvironment(std::wstring n) {
    environment_.emplace_back(std::move(n), std::nullopt);
    return *this;
}
ProcessBuilder& ProcessBuilder::RedirectStdout(bool v) noexcept {
    redirect_stdout_ = v;
    return *this;
}
ProcessBuilder& ProcessBuilder::RedirectStderr(bool v) noexcept {
    redirect_stderr_ = v;
    return *this;
}
ProcessBuilder& ProcessBuilder::MergeStderrIntoStdout(bool v) noexcept {
    merge_stderr_ = v;
    if (v) redirect_stdout_ = true;
    return *this;
}
ProcessBuilder& ProcessBuilder::RedirectStdin(bool v) noexcept {
    redirect_stdin_ = v;
    return *this;
}
ProcessBuilder& ProcessBuilder::NoWindow(bool v) noexcept {
    no_window_ = v;
    return *this;
}
ProcessBuilder& ProcessBuilder::NewProcessGroup(bool v) noexcept {
    new_process_group_ = v;
    return *this;
}
Result<Process> ProcessBuilder::Launch() const {
    auto copy = *this;
    copy.job_options_.reset();
    return copy.LaunchConfigured();
}
Result<SupervisedProcess> ProcessBuilder::LaunchSupervised(ProcessJobOptions options) const {
    auto copy = *this;
    copy.job_options_ = std::move(options);
    auto process = copy.LaunchConfigured();
    if (!process) return process.GetError();
    return SupervisedProcess(std::move(process.Value()));
}
Result<Process> ProcessBuilder::LaunchConfigured() const {
    if (executable_.empty())
        return SystemError::FromWin32(ERROR_INVALID_PARAMETER).WithOperation(L"Process executable");
    std::wstring command = QuoteWindowsArgument(executable_.wstring());
    for (const auto& arg : arguments_) {
        command.push_back(L' ');
        command += QuoteWindowsArgument(arg);
    }
    auto environment = BuildEnvironment(inherit_environment_, environment_);
    if (!environment) return environment.GetError();
    KernelHandle in_read, in_write, out_read, out_write, err_read, err_write;
    if (redirect_stdin_) {
        auto p = CreateInputPipe();
        if (!p) return p.GetError();
        in_read = std::move(p.Value().first);
        in_write = std::move(p.Value().second);
    }
    if (redirect_stdout_) {
        auto p = CreateOutputPipe();
        if (!p) return p.GetError();
        out_read = std::move(p.Value().first);
        out_write = std::move(p.Value().second);
    }
    if (redirect_stderr_ && !merge_stderr_) {
        auto p = CreateOutputPipe();
        if (!p) return p.GetError();
        err_read = std::move(p.Value().first);
        err_write = std::move(p.Value().second);
    }
    KernelHandle inherited_input, inherited_output, inherited_error;
    if (redirect_stdin_ || redirect_stdout_ || redirect_stderr_ || merge_stderr_) {
        if (!redirect_stdin_) {
            auto input =
                DuplicateInheritable(GetStdHandle(STD_INPUT_HANDLE), true, L"Duplicate stdin");
            if (!input) return input.GetError();
            inherited_input = std::move(input.Value());
        }
        if (!redirect_stdout_) {
            auto output =
                DuplicateInheritable(GetStdHandle(STD_OUTPUT_HANDLE), false, L"Duplicate stdout");
            if (!output) return output.GetError();
            inherited_output = std::move(output.Value());
        }
        if (!redirect_stderr_ && !merge_stderr_) {
            auto error =
                DuplicateInheritable(GetStdHandle(STD_ERROR_HANDLE), false, L"Duplicate stderr");
            if (!error) return error.GetError();
            inherited_error = std::move(error.Value());
        }
    }
    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb = sizeof(startup);
    startup.StartupInfo.dwFlags =
        redirect_stdin_ || redirect_stdout_ || redirect_stderr_ || merge_stderr_
            ? STARTF_USESTDHANDLES
            : 0;
    startup.StartupInfo.hStdOutput = redirect_stdout_ ? out_write.Get() : inherited_output.Get();
    startup.StartupInfo.hStdError = merge_stderr_ ? out_write.Get()
                                                  : (redirect_stderr_ ? err_write.Get()
                                                                      : inherited_error.Get());
    startup.StartupInfo.hStdInput = redirect_stdin_ ? in_read.Get() : inherited_input.Get();
    std::vector<HANDLE> inherited;
    if (out_write) inherited.push_back(out_write.Get());
    if (err_write) inherited.push_back(err_write.Get());
    if (in_read) inherited.push_back(in_read.Get());
    if (inherited_input) inherited.push_back(inherited_input.Get());
    if (inherited_output) inherited.push_back(inherited_output.Get());
    if (inherited_error) inherited.push_back(inherited_error.Get());
    std::vector<std::byte> attrs;
    if (!inherited.empty()) {
        SIZE_T bytes = 0;
        InitializeProcThreadAttributeList(nullptr, 1, 0, &bytes);
        attrs.resize(bytes);
        startup.lpAttributeList = reinterpret_cast<PPROC_THREAD_ATTRIBUTE_LIST>(attrs.data());
        if (!InitializeProcThreadAttributeList(startup.lpAttributeList, 1, 0, &bytes))
            return SystemError::LastWin32().WithOperation(L"InitializeProcThreadAttributeList");
        if (!UpdateProcThreadAttribute(startup.lpAttributeList, 0,
                                       PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherited.data(),
                                       inherited.size() * sizeof(HANDLE), nullptr, nullptr)) {
            DeleteProcThreadAttributeList(startup.lpAttributeList);
            return SystemError::LastWin32().WithOperation(L"UpdateProcThreadAttribute");
        }
    }
    KernelHandle job;
    if (job_options_) {
        auto created = CreateJob(*job_options_);
        if (!created) {
            if (startup.lpAttributeList) DeleteProcThreadAttributeList(startup.lpAttributeList);
            return created.GetError();
        }
        job = std::move(created.Value());
    }
    PROCESS_INFORMATION info{};
    std::vector<wchar_t> mutable_command(command.begin(), command.end());
    mutable_command.push_back(L'\0');
    const auto executable = executable_.wstring(), directory = working_directory_.wstring();
    DWORD flags = CREATE_UNICODE_ENVIRONMENT | EXTENDED_STARTUPINFO_PRESENT;
    if (job) flags |= CREATE_SUSPENDED;
    if (new_process_group_) flags |= CREATE_NEW_PROCESS_GROUP;
    if (no_window_) flags |= CREATE_NO_WINDOW;
    const BOOL launched = CreateProcessW(
        executable.c_str(), mutable_command.data(), nullptr, nullptr, !inherited.empty(), flags,
        environment.Value().data(), directory.empty() ? nullptr : directory.c_str(),
        &startup.StartupInfo, &info);
    if (startup.lpAttributeList) DeleteProcThreadAttributeList(startup.lpAttributeList);
    if (!launched) return SystemError::LastWin32().WithOperation(L"CreateProcessW");
    KernelHandle process(info.hProcess), thread(info.hThread);
    out_write.Reset();
    err_write.Reset();
    in_read.Reset();
    if (job) {
        if (!AssignProcessToJobObject(job.Get(), process.Get())) {
            TerminateProcess(process.Get(), ERROR_PROCESS_ABORTED);
            return SystemError::LastWin32().WithOperation(L"AssignProcessToJobObject");
        }
        if (ResumeThread(thread.Get()) == static_cast<DWORD>(-1)) {
            TerminateJobObject(job.Get(), ERROR_PROCESS_ABORTED);
            return SystemError::LastWin32().WithOperation(L"ResumeThread");
        }
    }
    return Process(std::move(process), std::move(thread), std::move(job), std::move(in_write),
                   std::move(out_read), std::move(err_read), info.dwProcessId, new_process_group_);
}
}  // namespace mwfl
