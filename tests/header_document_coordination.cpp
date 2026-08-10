#include <mwfl/document_coordination.h>

void UseDocumentCoordinationHeader() {
    mwfl::DocumentWorkspaceModel workspace{{1}};
    static_cast<void>(mwfl::BuildActiveDocumentCommandProjection(workspace));
}
