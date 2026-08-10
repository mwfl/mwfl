#include <mwtl/shell_integration.h>

mwtl::ShellResult InstallFixtureTasks(const std::filesystem::path& executable) {
    mwtl::JumpListCommitOptions options;
    options.app_id = L"mwtl.agent.fixture";
    options.executable = executable;
    options.tasks = {{L"open", L"Open fixture", L"--open", {}, 0}};
    return mwtl::CommitJumpList(options);
}
