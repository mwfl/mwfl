#include <mwfl/document_workspace.h>

void UseDocumentWorkspaceHeader() {
    mwfl::DocumentWorkspaceModel workspace{{1}};
    static_cast<void>(workspace.GetCount());
}
