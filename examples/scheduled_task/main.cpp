#include <mwfl/scheduler.h>
#include <iostream>
int wmain(int argc, wchar_t** argv) {
    wchar_t executable[MAX_PATH]{}; if (!GetModuleFileNameW(nullptr, executable, MAX_PATH)) return 1;
    mwfl::TaskDefinition spec{L"\\mwfl", L"example-" + std::to_wstring(GetCurrentProcessId()), L"MWFL explicit integration task", executable, {}};
    if (argc == 2 && std::wstring_view(argv[1]) == L"--integration") {
        mwfl::TaskScheduler scheduler;
        (void)scheduler.Remove(spec.folder, spec.name);
        auto plan = scheduler.Plan(spec);
        auto installed = plan ? scheduler.Apply(plan.Value())
                              : mwfl::Result<mwfl::TaskApplyResult>{plan.GetError()};
        if (!installed) { (void)scheduler.Remove(spec.folder, spec.name); return 2; }
        auto repeated_plan = scheduler.Plan(spec);
        if (!repeated_plan || repeated_plan.Value().required) {
            (void)scheduler.Remove(spec.folder, spec.name); return 3;
        }
        auto removed = scheduler.Remove(spec.folder, spec.name);
        auto audit = scheduler.Query(spec.folder, spec.name);
        return removed && removed.Value() && audit && !audit.Value().exists ? 0 : 4;
    }
    if (argc != 2 || std::wstring_view(argv[1]) != L"--self-test") return 0;
    if (!mwfl::ValidateScheduledTask(spec)) return 4; std::wcout << L"scheduled task model passed\n"; return 0;
}
