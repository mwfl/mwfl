#include <mwtl/shell_integration.h>

mwtl::ShellResult ApplyFixtureProgress(mwtl::TaskbarWindowIntegration& taskbar,
                                       std::uint64_t completed) {
    mwtl::TaskbarProgressModel model;
    if (!model.SetValue(completed, 100))
        return {mwtl::ShellStatus::invalid_argument, E_INVALIDARG};
    model.SetState(mwtl::TaskbarProgressState::normal);
    return taskbar.Apply(model);
}
