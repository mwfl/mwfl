#include <mwfl/service.h>
#include <iostream>
#include <string_view>
namespace {
class ExampleService final : public mwfl::ServiceApplication {
  public:
    explicit ExampleService(bool single_pass = false) : single_pass_(single_pass) {}
    mwfl::Result<mwfl::ServiceExit> Run(mwfl::ServiceContext& context) override {
        if (single_pass_) return mwfl::ServiceExit{};
        while (!context.StopToken().stop_requested()) Sleep(50);
        return mwfl::ServiceExit{};
    }
  private:
    bool single_pass_;
};
}
int wmain(int argc, wchar_t** argv) {
    if (argc == 2 && std::wstring_view(argv[1]) == L"--service") {
        ExampleService application;
        return mwfl::RunWindowsService({L"mwfl-example", L"MWFL service example"}, application);
    }
    if (argc != 2 || std::wstring_view(argv[1]) != L"--self-test") {
        std::wcout << L"Usage: mwfl_service_host --self-test | --service\n"; return 0;
    }
    mwfl::ServiceStateMachine state;
    if (!state.Transition(mwfl::ServiceState::StartPending) || !state.Transition(mwfl::ServiceState::Running)) return 1;
    ExampleService application(true);
    const int result =
        mwfl::RunServiceConsole({L"mwfl-example", L"MWFL service example"}, application);
    if (result != 0) return 2;
    if (!state.Transition(mwfl::ServiceState::StopPending) || !state.Transition(mwfl::ServiceState::Stopped)) return 3;
    std::wcout << L"service lifecycle and console host passed\n";
    return 0;
}
