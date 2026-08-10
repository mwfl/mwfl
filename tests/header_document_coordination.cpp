#include <mwtl/document_coordination.h>

void UseDocumentCoordinationHeader() {
    mwtl::DocumentWorkspaceModel workspace{{1}};
    static_cast<void>(mwtl::BuildActiveDocumentCommandProjection(workspace));
}
