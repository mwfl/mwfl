#include <mwfl/process.h>
#include <array>
#include <chrono>
#include <string>
namespace {
int RunChildMode(int argc, wchar_t** argv) {
    if (argc < 2) return -1;
    if (std::wstring_view(argv[1]) == L"--sleep-child") {
        ::Sleep(3000);
        return 0;
    }
    if (std::wstring_view(argv[1]) != L"--hold-output") return -1;
    wchar_t executable[MAX_PATH]{};
    if (!::GetModuleFileNameW(nullptr, executable, MAX_PATH)) return 90;
    std::wstring command = mwfl::QuoteWindowsArgument(executable) + L" --sleep-child";
    STARTUPINFOW startup{sizeof(startup)};
    PROCESS_INFORMATION process{};
    if (!::CreateProcessW(executable, command.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW,
                          nullptr, nullptr, &startup, &process))
        return 91;
    ::CloseHandle(process.hThread);
    ::CloseHandle(process.hProcess);
    return 0;
}
}  // namespace
int wmain(int argc, wchar_t** argv) {
    if (argc > 1) return RunChildMode(argc, argv);
    if (mwfl::QuoteWindowsArgument(L"plain") != L"plain" ||
        mwfl::QuoteWindowsArgument(L"") != L"\"\"" ||
        mwfl::QuoteWindowsArgument(L"a b") != L"\"a b\"" ||
        mwfl::QuoteWindowsArgument(L"a\"b") != L"\"a\\\"b\"")
        return 1;
    wchar_t command_processor[MAX_PATH]{};
    if (GetEnvironmentVariableW(L"ComSpec", command_processor, MAX_PATH) == 0) return 2;
    auto process = mwfl::ProcessBuilder{}
                       .Executable(command_processor)
                       .Argument(L"/d")
                       .Argument(L"/q")
                       .Argument(L"/c")
                       .Argument(L"more")
                       .RedirectStdin()
                       .RedirectStdout()
                       .Launch();
    if (!process) return 3;
    constexpr std::string_view input = "foundation-input\r\n";
    auto written = process.Value().WriteInput(std::as_bytes(std::span(input.data(), input.size())),
                                              mwfl::Deadline::After(std::chrono::seconds(2)));
    if (!written || written.Value().status != mwfl::CompletionStatus::Completed ||
        *written.Value().value != input.size())
        return 4;
    process.Value().CloseInput();
    auto output = process.Value().CollectOutput(
        1024, 0, mwfl::Deadline::After(std::chrono::seconds(2)));
    if (!output || output.Value().status != mwfl::CompletionStatus::Completed) return 5;
    const auto& bytes = output.Value().value->stdout_bytes;
    const std::string text(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    if (text.find("foundation-input") == std::string::npos) return 6;

    auto incremental = mwfl::ProcessBuilder{}
                           .Executable(command_processor)
                           .Argument(L"/d")
                           .Argument(L"/q")
                           .Argument(L"/c")
                           .Argument(L"echo merged-output 1>&2")
                           .MergeStderrIntoStdout()
                           .LaunchSupervised();
    if (!incremental) return 7;
    std::array<std::byte, 128> buffer{};
    auto read = incremental.Value().ReadStdout(
        buffer, mwfl::Deadline::After(std::chrono::seconds(2)));
    if (!read || read.Value().status != mwfl::CompletionStatus::Completed ||
        !read.Value().value)
        return 8;
    const std::string merged(reinterpret_cast<const char*>(buffer.data()),
                             *read.Value().value);
    if (merged.find("merged-output") == std::string::npos) return 9;
    auto incremental_wait = incremental.Value().Wait(
        mwfl::Deadline::After(std::chrono::seconds(2)));
    if (!incremental_wait ||
        incremental_wait.Value().status != mwfl::CompletionStatus::Completed)
        return 10;

    wchar_t self[MAX_PATH]{};
    if (!GetModuleFileNameW(nullptr, self, MAX_PATH)) return 11;
    auto inherited = mwfl::ProcessBuilder{}
                         .Executable(self)
                         .Argument(L"--hold-output")
                         .RedirectStdout()
                         .LaunchSupervised();
    if (!inherited) return 12;
    const auto started = std::chrono::steady_clock::now();
    auto bounded = inherited.Value().RunUntilExit(
        1024, 0, mwfl::Deadline::After(std::chrono::milliseconds(150)));
    const auto elapsed = std::chrono::steady_clock::now() - started;
    const auto terminated = inherited.Value().TerminateTree(ERROR_PROCESS_ABORTED);
    if (!bounded || bounded.Value().status != mwfl::CompletionStatus::TimedOut || !terminated ||
        elapsed > std::chrono::seconds(2))
        return 13;
    return 0;
}
