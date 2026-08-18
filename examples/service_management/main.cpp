#include <mwfl/service.h>

#include <chrono>
#include <iostream>
#include <thread>

using namespace std::chrono_literals;

namespace {
struct ServiceCleanup {
    mwfl::ServiceManager* manager;
    std::wstring name;
    ~ServiceCleanup() {
        (void)manager->Stop(name, 2s);
        (void)manager->Remove(name);
    }
};
}  // namespace

int RunInstalledService(std::wstring name) {
    mwfl::ServiceDefinition definition{std::move(name), L"MWFL integration service", 10s, true};
    mwfl::ServiceCallbacks callbacks;
    callbacks.run = [](mwfl::ServiceContext& context) -> mwfl::Result<mwfl::ServiceExit> {
        while (!context.StopToken().stop_requested()) {
            const auto pause = context.WaitWhilePaused(100ms);
            if (pause == mwfl::WaitStatus::Cancelled) break;
            std::this_thread::sleep_for(20ms);
        }
        return mwfl::ServiceExit{};
    };
    callbacks.control = [](mwfl::ServiceContext&,
                           const mwfl::ServiceControlEvent&) -> mwfl::Result<void> { return {}; };
    return mwfl::RunWindowsService(definition, std::move(callbacks));
}

int RunIntegration(const wchar_t* executable) {
    const std::wstring name =
        L"mwfl-foundation-integration-" + std::to_wstring(GetCurrentProcessId());
    auto manager = mwfl::ServiceManager::Open(mwfl::ServiceManager::Access::Manage);
    if (!manager) return manager.Error().code;
    ServiceCleanup cleanup{&manager.Value(), name};
    (void)manager.Value().Remove(name);
    mwfl::ServiceInstallSpec spec{name,
                                  L"MWFL Foundation Integration",
                                  executable,
                                  {L"--service", name},
                                  mwfl::ServiceAccount::LocalSystem,
                                  SERVICE_DEMAND_START};
    auto installed = manager.Value().InstallOrUpdate(spec);
    if (!installed || !installed.Value().created) return 10;
    auto repeated = manager.Value().InstallOrUpdate(spec);
    if (!repeated || repeated.Value().changed) return 11;
    auto started = manager.Value().Start(name, 10s);
    if (!started || started.Value().status != mwfl::ServiceOperationStatus::ReachedTarget ||
        started.Value().snapshot.state != SERVICE_RUNNING)
        return 12;
    if (!manager.Value().SendControl(name, SERVICE_CONTROL_PAUSE)) return 13;
    if (!manager.Value().SendControl(name, SERVICE_CONTROL_CONTINUE)) return 14;
    if (!manager.Value().SendControl(name, 128)) return 15;
    auto stopped = manager.Value().Stop(name, 10s);
    if (!stopped || stopped.Value().status != mwfl::ServiceOperationStatus::ReachedTarget ||
        stopped.Value().snapshot.state != SERVICE_STOPPED)
        return 16;
    auto removed = manager.Value().Remove(name);
    if (!removed || !removed.Value()) return 17;
    auto audit = manager.Value().Query(name);
    return audit && audit.Value().status == mwfl::ServiceQueryStatus::NotFound ? 0 : 18;
}

int wmain(int argc, wchar_t** argv) {
    if (argc == 3 && std::wstring_view(argv[1]) == L"--service")
        return RunInstalledService(argv[2]);
    if (argc == 2 && std::wstring_view(argv[1]) == L"--integration") {
        wchar_t executable[MAX_PATH]{};
        if (!GetModuleFileNameW(nullptr, executable, MAX_PATH)) return GetLastError();
        return RunIntegration(executable);
    }
    if (argc != 2 || std::wstring_view(argv[1]) != L"--self-test") return 0;
    mwfl::ServiceStateMachine state;
    if (!state.Transition(mwfl::ServiceState::StartPending) ||
        !state.Transition(mwfl::ServiceState::Running))
        return 1;
    const int result = mwfl::RunServiceConsole(
        {L"mwfl-management", L"MWFL management"},
        mwfl::ServiceCallbacks{[](mwfl::ServiceContext&) -> mwfl::Result<mwfl::ServiceExit> {
            return mwfl::ServiceExit{};
        }});
    std::wcout << L"service context and explicit management boundary passed\n";
    return result;
}
