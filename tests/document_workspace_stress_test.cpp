#include <mwfl/mwfl.h>

#include <array>

int main() {
    using namespace mwfl;
    std::uint64_t next_id = 1;
    for (int cycle = 0; cycle < 250; ++cycle) {
        DocumentWorkspaceModel left{{1}, 16};
        DocumentWorkspaceModel right{{2}, 16};
        for (int index = 0; index < 24; ++index) {
            const DocumentId id{next_id++};
            if (!left.Add({id, L"Document " + std::to_wstring(id.value), {},
                           index % 3 == 0, index % 2 == 0, index % 5 == 0,
                           {static_cast<std::uint64_t>(index),
                            static_cast<std::uint64_t>(index + 1),
                            static_cast<std::uint64_t>(index), 1.0}})) return 1;
        }
        for (int index = 0; index < 12; ++index) {
            const auto id = left.GetDocuments().front().id;
            if (!TransferDocument(left, right, id)) return 2;
        }
        if (left.GetCount() != 12 || right.GetCount() != 12) return 3;
        for (std::size_t index = 0; index < right.GetCount(); ++index) {
            const auto id = right.GetDocuments()[index].id;
            if (!right.SetUndoState(id, true, index % 2 == 0) ||
                !right.SetViewState(id, {index, index + 2, index, 1.25})) return 4;
        }
        const auto moved = right.GetDocuments().back().id;
        if (!right.Move(moved, 0) || !right.Activate(moved) ||
            !TransferDocument(right, left, moved, 3)) return 5;

        DocumentSession session;
        session.workspaces.push_back(CaptureWorkspaceSession(left));
        session.workspaces.push_back(CaptureWorkspaceSession(right));
        const auto parsed = ParseDocumentSession(SerializeDocumentSession(session));
        if (!parsed || parsed.session->workspaces.size() != 2 ||
            parsed.session->workspaces[0].documents.size() != left.GetCount() ||
            parsed.session->workspaces[1].documents.size() != right.GetCount()) return 6;

        std::vector<DocumentCloseDecision> cancelled_decisions;
        for (const auto& document : left.GetDocuments()) {
            if (document.dirty)
                cancelled_decisions.push_back({document.id, DocumentCloseChoice::cancel});
        }
        const auto cancelled = BuildCoordinatedClosePlan(left, cancelled_decisions);
        if (cancelled.status != CoordinatedCloseStatus::cancelled ||
            left.GetCount() != 13) return 7;

        std::vector<DocumentCloseDecision> discard;
        for (const auto& document : left.GetDocuments())
            if (document.dirty)
                discard.push_back({document.id, DocumentCloseChoice::discard});
        const auto close = BuildCoordinatedClosePlan(left, discard);
        if (!close || !ExecuteCoordinatedClose(left, close, {}) ||
            left.GetCount() != 0 || left.GetActiveId()) return 8;
    }
    return 0;
}
