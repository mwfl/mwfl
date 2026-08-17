#include <mwfl/process.h>
#include <algorithm>
namespace mwfl {
std::wstring QuoteWindowsArgument(std::wstring_view argument) {
    if (!argument.empty() && argument.find_first_of(L" \t\"") == std::wstring_view::npos) return std::wstring(argument);
    std::wstring quoted(1, L'"');
    std::size_t backslashes = 0;
    for (wchar_t character : argument) {
        if (character == L'\\') { ++backslashes; continue; }
        if (character == L'"') {
            quoted.append(backslashes * 2 + 1, L'\\'); quoted.push_back(L'"'); backslashes = 0; continue;
        }
        quoted.append(backslashes, L'\\'); backslashes = 0; quoted.push_back(character);
    }
    quoted.append(backslashes * 2, L'\\'); quoted.push_back(L'"');
    return quoted;
}
Process::Process(KernelHandle process, KernelHandle thread, DWORD id) noexcept
    : process_(std::move(process)), thread_(std::move(thread)), id_(id) {}
Result<ProcessExit> Process::Wait(std::chrono::milliseconds timeout, std::stop_token stop) {
    auto waited = WaitForHandle(process_.Get(), timeout, stop);
    if (!waited) return waited.Error();
    if (waited.Value().status == WaitStatus::Timeout) return NativeError::FromWin32(WAIT_TIMEOUT);
    if (waited.Value().status == WaitStatus::Cancelled) return NativeError::FromWin32(ERROR_CANCELLED);
    DWORD code = 0;
    if (!GetExitCodeProcess(process_.Get(), &code)) return NativeError::LastWin32();
    return ProcessExit{code};
}
Result<void> Process::Terminate(DWORD exit_code) noexcept {
    if (!process_ || !TerminateProcess(process_.Get(), exit_code)) return NativeError::LastWin32();
    return {};
}
ProcessBuilder& ProcessBuilder::Executable(std::filesystem::path value) { executable_ = std::move(value); return *this; }
ProcessBuilder& ProcessBuilder::Argument(std::wstring value) { arguments_.push_back(std::move(value)); return *this; }
ProcessBuilder& ProcessBuilder::WorkingDirectory(std::filesystem::path value) { working_directory_ = std::move(value); return *this; }
ProcessBuilder& ProcessBuilder::NewProcessGroup(bool enabled) noexcept { new_process_group_ = enabled; return *this; }
Result<Process> ProcessBuilder::Launch() const {
    if (executable_.empty()) return NativeError::FromWin32(ERROR_INVALID_PARAMETER);
    std::wstring command = QuoteWindowsArgument(executable_.wstring());
    for (const auto& argument : arguments_) { command.push_back(L' '); command += QuoteWindowsArgument(argument); }
    STARTUPINFOW startup{}; startup.cb = sizeof(startup);
    PROCESS_INFORMATION info{};
    std::vector<wchar_t> mutable_command(command.begin(), command.end()); mutable_command.push_back(L'\0');
    const std::wstring executable = executable_.wstring();
    const std::wstring directory = working_directory_.wstring();
    const DWORD flags = new_process_group_ ? CREATE_NEW_PROCESS_GROUP : 0;
    if (!CreateProcessW(executable.c_str(), mutable_command.data(), nullptr, nullptr, FALSE, flags, nullptr,
                        directory.empty() ? nullptr : directory.c_str(), &startup, &info)) return NativeError::LastWin32();
    return Process(KernelHandle(info.hProcess), KernelHandle(info.hThread), info.dwProcessId);
}
}  // namespace mwfl
