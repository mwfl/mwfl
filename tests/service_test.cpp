#include <mwfl/service.h>
#include <cassert>
int main() {
    mwfl::ServiceStateMachine state;
    assert(!state.Transition(mwfl::ServiceState::Running));
    assert(state.Transition(mwfl::ServiceState::StartPending));
    assert(state.Transition(mwfl::ServiceState::Running));
    assert(state.Transition(mwfl::ServiceState::PausePending));
    assert(state.Transition(mwfl::ServiceState::Paused));
    assert(state.Transition(mwfl::ServiceState::ContinuePending));
    assert(state.Transition(mwfl::ServiceState::Running));
    assert(state.Transition(mwfl::ServiceState::StopPending));
    assert(state.Transition(mwfl::ServiceState::Stopped));
}
