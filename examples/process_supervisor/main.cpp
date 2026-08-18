#include <mwfl/process.h>
#include <chrono>
#include <iostream>
#include <string_view>
using namespace std::chrono_literals;
int wmain(int argc, wchar_t** argv) {
    if (argc == 2 && std::wstring_view(argv[1]) == L"--child") { std::cout << "worker-output"; std::cerr << "worker-error"; return 31; }
    if (argc != 2 || std::wstring_view(argv[1]) != L"--self-test") return 0;
    wchar_t path[MAX_PATH]{}; if (!GetModuleFileNameW(nullptr, path, MAX_PATH)) return 1;
    auto child = mwfl::ProcessBuilder{}.Executable(path).Argument(L"--child").Supervise()
        .RedirectStdout().RedirectStderr().Environment(L"MWFL_WORKER", L"1").Launch();
    if (!child || !child.Value().Supervised()) return 2;
    auto output = child.Value().CollectOutput(1024, 1024, 5s);
    if (!output || output.Value().process.status != mwfl::ProcessWaitStatus::Exited || output.Value().process.exit_code != 31) return 3;
    if (output.Value().stdout_bytes.empty() || output.Value().stderr_bytes.empty()) return 4;
    std::wcout << L"supervised process and bounded output passed\n"; return 0;
}
