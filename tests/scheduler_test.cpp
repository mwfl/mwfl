#include <mwfl/scheduler.h>
int main() {
    auto invalid = mwfl::ValidateScheduledTask({L"\\", L"task", L"", L"test.exe"});
    if (invalid) return 1;
    mwfl::TaskDefinition valid{L"\\mwfl", L"test", L"model", L"test.exe"};
    if (!mwfl::ValidateScheduledTask(valid)) return 2;
    valid.trigger = mwfl::ScheduledTaskTrigger::Once;
    return mwfl::ValidateScheduledTask(valid) ? 3 : 0;
}
