#include <mwtl/tab_workspace.h>

void HeaderTabWorkspaceCompiles() {
    mwtl::TabWorkspaceModel tabs;
    static_cast<void>(tabs.Add({{1}, L"Tab", false, true}));
}
