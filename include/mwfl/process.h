#pragma once
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <mwfl/core.h>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
namespace mwfl {
[[nodiscard]] std::wstring QuoteWindowsArgument(std::wstring_view argument);
enum class ProcessWaitStatus { Exited, TimedOut, Cancelled };
struct ProcessWaitResult {
    ProcessWaitStatus status = ProcessWaitStatus::TimedOut;
    DWORD exit_code = STILL_ACTIVE;
};
struct ProcessJobOptions {
    bool kill_on_close = true;
    std::optional<DWORD> active_process_limit;
    std::optional<std::size_t> process_memory_limit;
    std::optional<std::size_t> job_memory_limit;
};
struct ProcessOutput {
    ProcessWaitResult process;
    std::vector<std::byte> stdout_bytes;
    std::vector<std::byte> stderr_bytes;
    bool stdout_truncated = false;
    bool stderr_truncated = false;
};
enum class ProcessInputStatus { Completed, TimedOut, Cancelled, Disconnected };
struct ProcessInputResult {
    ProcessInputStatus status = ProcessInputStatus::Completed;
    std::size_t bytes_written = 0;
};
class Process final {
   public:
    Process() = default;
    Process(KernelHandle process, KernelHandle thread, KernelHandle job, KernelHandle stdin_write,
            KernelHandle stdout_read, KernelHandle stderr_read, DWORD id,
            bool new_process_group) noexcept;
    Process(Process&&) noexcept = default;
    Process& operator=(Process&&) noexcept = default;
    Process(const Process&) = delete;
    Process& operator=(const Process&) = delete;
    [[nodiscard]] DWORD Id() const noexcept { return id_; }
    [[nodiscard]] HANDLE NativeHandle() const noexcept { return process_.Get(); }
    [[nodiscard]] bool Supervised() const noexcept { return static_cast<bool>(job_); }
    [[nodiscard]] Result<ProcessWaitResult> Wait(std::chrono::milliseconds timeout,
                                                 std::stop_token stop = {}) const;
    [[nodiscard]] Result<void> RequestConsoleStop() const noexcept;
    [[nodiscard]] Result<void> Terminate(DWORD exit_code) noexcept;
    [[nodiscard]] Result<void> TerminateTree(DWORD exit_code) noexcept;
    [[nodiscard]] Result<ProcessOutput> CollectOutput(std::size_t maximum_stdout_bytes,
                                                      std::size_t maximum_stderr_bytes,
                                                      std::chrono::milliseconds timeout,
                                                      std::stop_token stop = {});
    [[nodiscard]] Result<ProcessInputResult> WriteInput(std::span<const std::byte> input,
                                                        std::chrono::milliseconds timeout,
                                                        std::stop_token stop = {});
    void CloseInput() noexcept { stdin_write_.Reset(); }

   private:
    KernelHandle process_, thread_, job_, stdin_write_, stdout_read_, stderr_read_;
    DWORD id_ = 0;
    bool new_process_group_ = false;
};
class ProcessBuilder final {
   public:
    ProcessBuilder& Executable(std::filesystem::path value);
    ProcessBuilder& Argument(std::wstring value);
    ProcessBuilder& WorkingDirectory(std::filesystem::path value);
    ProcessBuilder& InheritEnvironment(bool enabled = true) noexcept;
    ProcessBuilder& Environment(std::wstring name, std::wstring value);
    ProcessBuilder& RemoveEnvironment(std::wstring name);
    ProcessBuilder& RedirectStdout(bool enabled = true) noexcept;
    ProcessBuilder& RedirectStderr(bool enabled = true) noexcept;
    ProcessBuilder& RedirectStdin(bool enabled = true) noexcept;
    ProcessBuilder& NoWindow(bool enabled = true) noexcept;
    ProcessBuilder& NewProcessGroup(bool enabled = true) noexcept;
    ProcessBuilder& Supervise(ProcessJobOptions options = {});
    [[nodiscard]] Result<Process> Launch() const;

   private:
    std::filesystem::path executable_, working_directory_;
    std::vector<std::wstring> arguments_;
    std::vector<std::pair<std::wstring, std::optional<std::wstring>>> environment_;
    std::optional<ProcessJobOptions> job_options_;
    bool inherit_environment_ = true, redirect_stdout_ = false, redirect_stderr_ = false,
         redirect_stdin_ = false;
    bool no_window_ = false, new_process_group_ = false;
};
}  // namespace mwfl
