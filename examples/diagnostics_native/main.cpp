#include <fstream>
#include <iostream>
#include <mwfl/diagnostics.h>
int wmain(int argc, wchar_t** argv) {
    if (argc == 2 && std::wstring_view(argv[1]) == L"--integration") {
        const std::wstring source = L"mwfl-foundation-" + std::to_wstring(GetCurrentProcessId());
        wchar_t executable[MAX_PATH]{};
        if (!GetModuleFileNameW(nullptr, executable, MAX_PATH)) return 10;
        (void)mwfl::EventLogSourceManager::Remove(source);
        auto installed = mwfl::EventLogSourceManager::Install(source, executable);
        if (!installed) return 11;
        auto exists = mwfl::EventLogSourceManager::Exists(source);
        mwfl::EventLogSink sink(source);
        auto written =
            sink.Write({mwfl::EventLevel::Information, L"integration", 1, {{L"result", L"ok"}}});
        auto removed = mwfl::EventLogSourceManager::Remove(source);
        auto audit = mwfl::EventLogSourceManager::Exists(source);
        return exists && exists.Value() && written && removed && removed.Value() && audit &&
                       !audit.Value()
                   ? 0
                   : 12;
    }
    if (argc != 2 || std::wstring_view(argv[1]) != L"--self-test") return 0;
    const auto path = std::filesystem::temp_directory_path() /
                      (L"mwfl-diagnostics-" + std::to_wstring(GetCurrentProcessId()) + L".log");
    mwfl::DiagnosticPipeline pipeline;
    pipeline.Add(std::make_shared<mwfl::TraceLoggingSink>());
    pipeline.Add(std::make_shared<mwfl::BoundedFileSink>(path, 4096, 2));
    auto report = pipeline.Write({mwfl::EventLevel::Information,
                                  L"native",
                                  1,
                                  {{L"token", L"never-log-this", mwfl::FieldSensitivity::Secret}}});
    std::string text;
    {
        std::ifstream input(path, std::ios::binary);
        std::getline(input, text, '\0');
    }
    std::filesystem::remove(path);
    if (report.failed || text.find("never-log-this") != std::string::npos ||
        text.find("<secret>") == std::string::npos)
        return 1;
    const auto dump = path.parent_path() /
                      (L"mwfl-diagnostics-" + std::to_wstring(GetCurrentProcessId()) + L".dmp");
    std::filesystem::remove(dump);
    auto written = mwfl::WriteMiniDump(dump, mwfl::MiniDumpKind::Normal);
    auto duplicate = mwfl::WriteMiniDump(dump, mwfl::MiniDumpKind::Normal);
    const bool dump_ok = written && std::filesystem::file_size(dump) > 0 && !duplicate &&
                         duplicate.GetError().code == ERROR_FILE_EXISTS;
    std::filesystem::remove(dump);
    if (!dump_ok) return 2;
    std::wcout << L"native diagnostics redaction passed\n";
    return 0;
}
