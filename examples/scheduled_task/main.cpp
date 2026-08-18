#include <mwfl/scheduler.h>
#include <iostream>
int wmain(int argc, wchar_t** argv) {
    wchar_t executable[MAX_PATH]{}; if (!GetModuleFileNameW(nullptr, executable, MAX_PATH)) return 1;
    mwfl::ScheduledTaskSpec spec{L"\\mwfl", L"example-" + std::to_wstring(GetCurrentProcessId()), L"MWFL explicit integration task", executable, {}};
    if (argc == 2 && std::wstring_view(argv[1]) == L"--integration") { mwfl::TaskScheduler scheduler; (void)scheduler.Remove(spec.folder, spec.name); auto installed = scheduler.InstallOrUpdate(spec); if (!installed) { (void)scheduler.Remove(spec.folder, spec.name); return 2; } auto repeated = scheduler.InstallOrUpdate(spec); if (!repeated || repeated.Value().changed) { (void)scheduler.Remove(spec.folder, spec.name); return 3; } auto removed = scheduler.Remove(spec.folder, spec.name); auto audit = scheduler.Query(spec.folder, spec.name); return removed && removed.Value() && audit && !audit.Value().exists ? 0 : 4; }
    if (argc != 2 || std::wstring_view(argv[1]) != L"--self-test") return 0;
    if (!mwfl::ValidateScheduledTask(spec)) return 4; std::wcout << L"scheduled task model passed\n"; return 0;
}
