#include <mwfl/service.h>
#include <iostream>
#include <string_view>
int wmain(int argc, wchar_t** argv) {
    if (argc == 2 && std::wstring_view(argv[1]) == L"--service") {
        return mwfl::RunWindowsService(
            {L"mwfl-example", L"MWFL service example"},
            [](std::stop_token stop) -> mwfl::Result<void> {
                while (!stop.stop_requested()) Sleep(50);
                return {};
            });
    }
    if (argc != 2 || std::wstring_view(argv[1]) != L"--self-test") {
        std::wcout << L"Usage: mwfl_service_host --self-test | --service\n"; return 0;
    }
    mwfl::ServiceStateMachine state;
    if (!state.Transition(mwfl::ServiceState::StartPending) || !state.Transition(mwfl::ServiceState::Running)) return 1;
    const int result = mwfl::RunServiceConsole({L"mwfl-example", L"MWFL service example"}, [](std::stop_token stop) -> mwfl::Result<void> {
        if (stop.stop_requested()) return mwfl::NativeError{mwfl::ErrorDomain::Application, 1};
        return {};
    });
    if (result != 0) return 2;
    if (!state.Transition(mwfl::ServiceState::StopPending) || !state.Transition(mwfl::ServiceState::Stopped)) return 3;
    std::wcout << L"service lifecycle and console host passed\n";
    return 0;
}
