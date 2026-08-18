#include <mwfl/core.h>
#include <mwfl/deployment.h>
#include <mwfl/diagnostics.h>
#include <mwfl/ipc.h>
#include <mwfl/process.h>
#include <mwfl/scheduler.h>
#include <mwfl/security.h>
#include <mwfl/service.h>
int main() {
    mwfl::TaskDefinition task{L"\\mwfl", L"consumer", L"", L"test.exe"};
    return mwfl::Utf8ToWide("foundation") && mwfl::ValidateScheduledTask(task) ? 0 : 1;
}
