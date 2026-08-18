#include <mwfl/process.h>
#include <string>
int main() {
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
                                              std::chrono::seconds(2));
    if (!written || written.Value().status != mwfl::ProcessInputStatus::Completed ||
        written.Value().bytes_written != input.size())
        return 4;
    process.Value().CloseInput();
    auto output = process.Value().CollectOutput(1024, 0, std::chrono::seconds(2));
    if (!output || output.Value().process.status != mwfl::ProcessWaitStatus::Exited) return 5;
    const std::string text(reinterpret_cast<const char*>(output.Value().stdout_bytes.data()),
                           output.Value().stdout_bytes.size());
    return text.find("foundation-input") != std::string::npos ? 0 : 6;
}
