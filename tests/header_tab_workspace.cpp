#include <mwfl/tab_workspace.h>

void HeaderTabWorkspaceCompiles() {
    mwfl::TabWorkspaceModel tabs;
    static_cast<void>(tabs.Add({{1}, L"Tab", false, true}));
}
