#include <mwfl/process.h>
#include <chrono>
#include <iostream>
#include <string_view>
using namespace std::chrono_literals;
int wmain(int argc, wchar_t** argv) {
    if (argc == 2 && std::wstring_view(argv[1]) == L"--child") { std::cout << "worker-output"; std::cerr << "worker-error"; return 31; }
    if (argc != 2 || std::wstring_view(argv[1]) != L"--self-test") return 0;
    wchar_t path[MAX_PATH]{}; if (!GetModuleFileNameW(nullptr, path, MAX_PATH)) return 1;
    auto child = mwfl::ProcessBuilder{}.Executable(path).Argument(L"--child")
        .RedirectStdout().RedirectStderr().Environment(L"MWFL_WORKER", L"1").LaunchSupervised();
    if (!child) return 2;
    auto output = child.Value().RunUntilExit(1024, 1024, mwfl::Deadline::After(5s));
    if (!output || output.Value().status != mwfl::CompletionStatus::Completed ||
        output.Value().value->exit_code != 31) return 3;
    if (output.Value().value->stdout_bytes.empty() || output.Value().value->stderr_bytes.empty()) return 4;
    std::wcout << L"supervised process and bounded output passed\n"; return 0;
}
