#include <mwfl/service.h>

#include <chrono>
#include <iostream>
#include <thread>

using namespace std::chrono_literals;

namespace {
class IntegrationService final : public mwfl::ServiceApplication {
  public:
    mwfl::Result<mwfl::ServiceExit> Run(mwfl::ServiceContext& context) override {
        while (!context.StopToken().stop_requested()) {
            const auto pause = context.WaitWhilePaused(mwfl::Deadline::After(100ms));
            if (pause.status == mwfl::CompletionStatus::Cancelled) break;
            std::this_thread::sleep_for(20ms);
        }
        return mwfl::ServiceExit{};
    }
};
class OneShotService final : public mwfl::ServiceApplication {
  public:
    mwfl::Result<mwfl::ServiceExit> Run(mwfl::ServiceContext&) override {
        return mwfl::ServiceExit{};
    }
};
struct ServiceCleanup {
    mwfl::ServiceManager* manager;
    std::wstring name;
    ~ServiceCleanup() {
        (void)manager->Stop(name, mwfl::Deadline::After(2s));
        (void)manager->Remove(name);
    }
};
}  // namespace

int RunInstalledService(std::wstring name) {
    mwfl::ServiceDefinition definition{std::move(name), L"MWFL integration service", 10s, true};
    IntegrationService application;
    return mwfl::RunWindowsService(definition, application);
}

int RunIntegration(const wchar_t* executable) {
    const std::wstring name =
        L"mwfl-foundation-integration-" + std::to_wstring(GetCurrentProcessId());
    auto manager = mwfl::ServiceManager::Open(mwfl::ServiceManager::Access::Manage);
    if (!manager) return manager.GetError().code;
    ServiceCleanup cleanup{&manager.Value(), name};
    (void)manager.Value().Remove(name);
    mwfl::ServiceInstallSpec spec{name,
                                  L"MWFL Foundation Integration",
                                  executable,
                                  {L"--service", name},
                                  mwfl::ServiceAccount::LocalSystem,
                                  SERVICE_DEMAND_START};
    auto install_plan = manager.Value().Plan(spec);
    auto installed = install_plan ? manager.Value().Apply(install_plan.Value())
                                  : mwfl::Result<mwfl::ServiceMutationResult>{install_plan.GetError()};
    if (!installed || !installed.Value().created) return 10;
    auto repeated = manager.Value().Plan(spec);
    if (!repeated || repeated.Value().required) return 11;
    auto started = manager.Value().Start(name, mwfl::Deadline::After(10s));
    if (!started || started.Value().status != mwfl::CompletionStatus::Completed ||
        started.Value().value->state != SERVICE_RUNNING)
        return 12;
    if (!manager.Value().SendControl(name, SERVICE_CONTROL_PAUSE)) return 13;
    if (!manager.Value().SendControl(name, SERVICE_CONTROL_CONTINUE)) return 14;
    if (!manager.Value().SendControl(name, 128)) return 15;
    auto stopped = manager.Value().Stop(name, mwfl::Deadline::After(10s));
    if (!stopped || stopped.Value().status != mwfl::CompletionStatus::Completed ||
        stopped.Value().value->state != SERVICE_STOPPED)
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
    OneShotService application;
    const int result = mwfl::RunServiceConsole(
        {L"mwfl-management", L"MWFL management"}, application);
    std::wcout << L"service context and explicit management boundary passed\n";
    return result;
}
