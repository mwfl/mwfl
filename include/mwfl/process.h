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
struct ProcessJobOptions {
    bool kill_on_close = true;
    std::optional<DWORD> active_process_limit;
    std::optional<std::size_t> process_memory_limit;
    std::optional<std::size_t> job_memory_limit;
    std::optional<DWORD> cpu_rate_hard_cap;
};
struct ProcessOutput {
    DWORD exit_code = STILL_ACTIVE;
    std::vector<std::byte> stdout_bytes;
    std::vector<std::byte> stderr_bytes;
    bool stdout_truncated = false;
    bool stderr_truncated = false;
};
class SupervisedProcess;
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
    [[nodiscard]] Result<OperationOutcome<DWORD>> Wait(Deadline deadline,
                                                        std::stop_token stop = {}) const;
    [[nodiscard]] Result<void> RequestConsoleStop() const noexcept;
    [[nodiscard]] Result<OperationOutcome<ProcessOutput>> CollectOutput(
        std::size_t maximum_stdout_bytes, std::size_t maximum_stderr_bytes, Deadline deadline,
        std::stop_token stop = {});
    [[nodiscard]] Result<OperationOutcome<std::size_t>> WriteInput(
        std::span<const std::byte> input, Deadline deadline, std::stop_token stop = {});
    [[nodiscard]] Result<OperationOutcome<std::size_t>> ReadStdout(
        std::span<std::byte> output, Deadline deadline, std::stop_token stop = {});
    [[nodiscard]] Result<OperationOutcome<std::size_t>> ReadStderr(
        std::span<std::byte> output, Deadline deadline, std::stop_token stop = {});
    void CloseInput() noexcept { stdin_write_.Reset(); }

   private:
    friend class SupervisedProcess;
    [[nodiscard]] Result<void> TerminateTree(DWORD exit_code) noexcept;
    KernelHandle process_, thread_, job_, stdin_write_, stdout_read_, stderr_read_;
    DWORD id_ = 0;
    bool new_process_group_ = false;
};
class SupervisedProcess final {
   public:
    SupervisedProcess() = default;
    explicit SupervisedProcess(Process process) : process_(std::move(process)) {}
    SupervisedProcess(SupervisedProcess&&) noexcept = default;
    SupervisedProcess& operator=(SupervisedProcess&&) noexcept = default;
    SupervisedProcess(const SupervisedProcess&) = delete;
    SupervisedProcess& operator=(const SupervisedProcess&) = delete;
    [[nodiscard]] DWORD Id() const noexcept { return process_.Id(); }
    [[nodiscard]] HANDLE NativeHandle() const noexcept { return process_.NativeHandle(); }
    [[nodiscard]] Result<OperationOutcome<DWORD>> Wait(Deadline deadline,
                                                        std::stop_token stop = {}) const {
        return process_.Wait(deadline, stop);
    }
    [[nodiscard]] Result<void> RequestConsoleStop() const noexcept {
        return process_.RequestConsoleStop();
    }
    [[nodiscard]] Result<void> TerminateTree(DWORD exit_code) noexcept {
        return process_.TerminateTree(exit_code);
    }
    [[nodiscard]] Result<OperationOutcome<ProcessOutput>> RunUntilExit(
        std::size_t maximum_stdout_bytes, std::size_t maximum_stderr_bytes, Deadline deadline,
        std::stop_token stop = {}) {
        return process_.CollectOutput(maximum_stdout_bytes, maximum_stderr_bytes, deadline, stop);
    }
    [[nodiscard]] Result<OperationOutcome<std::size_t>> WriteInput(
        std::span<const std::byte> input, Deadline deadline, std::stop_token stop = {}) {
        return process_.WriteInput(input, deadline, stop);
    }
    [[nodiscard]] Result<OperationOutcome<std::size_t>> ReadStdout(
        std::span<std::byte> output, Deadline deadline, std::stop_token stop = {}) {
        return process_.ReadStdout(output, deadline, stop);
    }
    [[nodiscard]] Result<OperationOutcome<std::size_t>> ReadStderr(
        std::span<std::byte> output, Deadline deadline, std::stop_token stop = {}) {
        return process_.ReadStderr(output, deadline, stop);
    }
    void CloseInput() noexcept { process_.CloseInput(); }

   private:
    Process process_;
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
    ProcessBuilder& MergeStderrIntoStdout(bool enabled = true) noexcept;
    ProcessBuilder& RedirectStdin(bool enabled = true) noexcept;
    ProcessBuilder& NoWindow(bool enabled = true) noexcept;
    ProcessBuilder& NewProcessGroup(bool enabled = true) noexcept;
    [[nodiscard]] Result<Process> Launch() const;
    [[nodiscard]] Result<SupervisedProcess> LaunchSupervised(
        ProcessJobOptions options = {}) const;

   private:
    std::filesystem::path executable_, working_directory_;
    std::vector<std::wstring> arguments_;
    std::vector<std::pair<std::wstring, std::optional<std::wstring>>> environment_;
    std::optional<ProcessJobOptions> job_options_;
    bool inherit_environment_ = true, redirect_stdout_ = false, redirect_stderr_ = false,
         redirect_stdin_ = false;
    bool no_window_ = false, new_process_group_ = false, merge_stderr_ = false;
    [[nodiscard]] Result<Process> LaunchConfigured() const;
};
}  // namespace mwfl
