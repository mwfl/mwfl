#include <mwtl/tray_icon.h>

int main() {
    using mwtl::TrayIconState;
    using mwtl::TrayIconStateModel;

    TrayIconStateModel state;
    if (state.GetState() != TrayIconState::detached || !state.CanAdd() || state.CanModify() ||
        state.CanRecreate() || state.BeginRecovery() || state.RegistrationSucceeded() ||
        state.InitialRegistrationFailed())
        return 1;

    if (!state.BeginAdd() || state.GetState() != TrayIconState::recovery_pending ||
        state.BeginAdd() || state.CanAdd() || state.CanModify() || !state.CanRecreate())
        return 2;
    if (!state.RegistrationSucceeded() || state.GetState() != TrayIconState::added ||
        !state.CanModify() || !state.CanRecreate() || state.RegistrationSucceeded())
        return 3;

    if (!state.BeginRecovery() || state.GetState() != TrayIconState::recovery_pending ||
        state.CanModify() || !state.CanRecreate())
        return 4;
    // A recovery failure intentionally remains retryable.
    if (!state.BeginRecovery() || !state.CanRecreate()) return 5;
    if (!state.RegistrationSucceeded() || !state.CanModify()) return 6;

    state.Detach();
    if (state.GetState() != TrayIconState::detached || state.CanRecreate()) return 7;
    if (!state.BeginAdd() || !state.InitialRegistrationFailed() ||
        state.GetState() != TrayIconState::detached)
        return 8;

    return 0;
}
