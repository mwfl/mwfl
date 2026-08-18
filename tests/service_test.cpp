#include <mwfl/service.h>
int main() {
    mwfl::ServiceStateMachine state;
    if (state.Transition(mwfl::ServiceState::Running)) return 1;
    if (!state.Transition(mwfl::ServiceState::StartPending)) return 2;
    if (!state.Transition(mwfl::ServiceState::Running)) return 3;
    if (!state.Transition(mwfl::ServiceState::PausePending)) return 4;
    if (!state.Transition(mwfl::ServiceState::Paused)) return 5;
    if (!state.Transition(mwfl::ServiceState::ContinuePending)) return 6;
    if (!state.Transition(mwfl::ServiceState::Running)) return 7;
    if (!state.Transition(mwfl::ServiceState::StopPending)) return 8;
    return state.Transition(mwfl::ServiceState::Stopped) ? 0 : 9;
}
