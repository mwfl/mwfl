#include <mwfl/diagnostics.h>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string_view>
int wmain(int argc, wchar_t** argv) {
    if (argc != 2 || std::wstring_view(argv[1]) != L"--self-test") { std::wcout << L"Usage: mwfl_diagnostics_pipeline --self-test\n"; return 0; }
    auto path = std::filesystem::temp_directory_path() / (L"mwfl-diagnostics-" + std::to_wstring(GetCurrentProcessId()) + L".log");
    std::error_code ignored; std::filesystem::remove(path, ignored);
    mwfl::DiagnosticPipeline pipeline; pipeline.Add(std::make_shared<mwfl::DebugOutputSink>()); pipeline.Add(std::make_shared<mwfl::BoundedFileSink>(path, 4096));
    mwfl::DiagnosticEvent event{mwfl::EventLevel::Information, L"example", 100, {{L"operation", L"startup"}, {L"account", L"Ada", mwfl::FieldSensitivity::Sensitive}, {L"password", L"never-log-me", mwfl::FieldSensitivity::Secret}}};
    auto written = pipeline.Write(event); if (!written) return 1;
    std::ifstream input(path, std::ios::binary); std::string content((std::istreambuf_iterator<char>(input)), {});
    std::filesystem::remove(path, ignored);
    if (content.find("startup") == std::string::npos || content.find("Ada") != std::string::npos || content.find("never-log-me") != std::string::npos) return 2;
    std::wcout << L"structured diagnostics and redaction passed\n"; return 0;
}
