#include <mwfl/shell_integration.h>

mwfl::ShellResult ApplyFixtureProgress(mwfl::TaskbarWindowIntegration& taskbar,
                                       std::uint64_t completed) {
    mwfl::TaskbarProgressModel model;
    if (!model.SetValue(completed, 100))
        return {mwfl::ShellStatus::invalid_argument, E_INVALIDARG};
    model.SetState(mwfl::TaskbarProgressState::normal);
    return taskbar.Apply(model);
}
