#include <iostream>
#include <span>
#include <string_view>

namespace {
int Run(std::span<wchar_t*> arguments) {
    if (arguments.empty() || std::wstring_view(arguments.front()) == L"--help") {
        std::wcout << L"Usage: mwfl_cli_basic echo <text>\n";
        return 0;
    }
    if (std::wstring_view(arguments.front()) == L"echo" && arguments.size() == 2) {
        std::wcout << arguments[1] << L'\n';
        return 0;
    }
    std::wcerr << L"Unknown command. Run with --help.\n";
    return 2;
}
} // namespace

int wmain(int argc, wchar_t** argv) {
    return Run({argv + 1, static_cast<std::size_t>(argc - 1)});
}
