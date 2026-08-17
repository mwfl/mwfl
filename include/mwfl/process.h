#pragma once
#include <mwfl/core.h>
#include <chrono>
#include <filesystem>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>
namespace mwfl {
[[nodiscard]] std::wstring QuoteWindowsArgument(std::wstring_view argument);
struct ProcessExit { DWORD code = STILL_ACTIVE; };
class Process final {
public:
    Process() = default;
    Process(KernelHandle process, KernelHandle thread, DWORD id) noexcept;
    Process(Process&&) noexcept = default;
    Process& operator=(Process&&) noexcept = default;
    Process(const Process&) = delete;
    Process& operator=(const Process&) = delete;
    [[nodiscard]] DWORD Id() const noexcept { return id_; }
    [[nodiscard]] HANDLE NativeHandle() const noexcept { return process_.Get(); }
    [[nodiscard]] Result<ProcessExit> Wait(std::chrono::milliseconds timeout, std::stop_token stop = {});
    [[nodiscard]] Result<void> Terminate(DWORD exit_code) noexcept;
private:
    KernelHandle process_;
    KernelHandle thread_;
    DWORD id_ = 0;
};
class ProcessBuilder final {
public:
    ProcessBuilder& Executable(std::filesystem::path value);
    ProcessBuilder& Argument(std::wstring value);
    ProcessBuilder& WorkingDirectory(std::filesystem::path value);
    ProcessBuilder& NewProcessGroup(bool enabled = true) noexcept;
    [[nodiscard]] Result<Process> Launch() const;
private:
    std::filesystem::path executable_;
    std::vector<std::wstring> arguments_;
    std::filesystem::path working_directory_;
    bool new_process_group_ = false;
};
}  // namespace mwfl
