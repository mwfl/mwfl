#include <mwfl/process.h>
#include <chrono>
#include <iostream>
#include <string_view>
using namespace std::chrono_literals;
int wmain(int argc, wchar_t** argv) {
    if (argc == 3 && std::wstring_view(argv[1]) == L"--child") return std::wstring_view(argv[2]) == L"argument with spaces" ? 17 : 18;
    if (argc != 2 || std::wstring_view(argv[1]) != L"--self-test") { std::wcout << L"Usage: mwfl_process_runner --self-test\n"; return 0; }
    wchar_t path[MAX_PATH]{}; if (GetModuleFileNameW(nullptr, path, MAX_PATH) == 0) return 1;
    auto launched = mwfl::ProcessBuilder{}.Executable(path).Argument(L"--child").Argument(L"argument with spaces").Launch();
    if (!launched) return 2;
    auto exited = launched.Value().Wait(5s);
    if (!exited || exited.Value().code != 17) return 3;
    std::wcout << L"process quoting, launch, wait, and exit passed\n"; return 0;
}
