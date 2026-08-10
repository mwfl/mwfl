#include <mwtl/document_workspace.h>

void UseDocumentWorkspaceHeader() {
    mwtl::DocumentWorkspaceModel workspace{{1}};
    static_cast<void>(workspace.GetCount());
}
