#include <mwfl/shell_integration.h>

mwfl::ShellResult InstallFixtureTasks(const std::filesystem::path& executable) {
    mwfl::JumpListCommitOptions options;
    options.app_id = L"mwfl.agent.fixture";
    options.executable = executable;
    options.tasks = {{L"open", L"Open fixture", L"--open", {}, 0}};
    return mwfl::CommitJumpList(options);
}
