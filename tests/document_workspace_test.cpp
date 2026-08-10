#include <mwfl/document_workspace.h>

#include <array>
#include <cmath>
#include <stdexcept>

int main() {
    using namespace mwfl;
    bool threw = false;
    try { DocumentWorkspaceModel invalid{{}}; } catch (const std::invalid_argument&) { threw = true; }
    if (!threw) return 1;

    DocumentWorkspaceModel left{{1}, 2};
    DocumentWorkspaceModel right{{2}, 2};
    WorkspaceDocument first{{11}, L"First", L"C:\\Docs\\First.txt"};
    WorkspaceDocument second{{22}, L"Second", L"C:\\Docs\\Second.txt", true, true, false,
                             {3, 8, 12, 1.25}};
    WorkspaceDocument third{{33}, L"Third", {}};
    if (!left.Add(first) || !left.Add(second) || !left.Add(third, 1) ||
        left.GetCount() != 3 || left.GetActiveId() != first.id) return 2;
    if (left.Add(first).status != DocumentWorkspaceStatus::duplicate_id) return 3;
    auto duplicate_path = third;
    duplicate_path.id = {44};
    duplicate_path.path = L"c:/docs/sub/../FIRST.TXT";
    if (left.Add(duplicate_path).status != DocumentWorkspaceStatus::duplicate_path) return 4;

    if (!left.Activate(second.id) || !left.SetUndoState(second.id, false, true) ||
        !left.SetDirty(second.id, false) || !left.SetViewState(second.id, {4, 9, 15, 1.5}) ||
        !left.Move(second.id, 0)) return 5;
    const auto* updated = left.Find(second.id);
    if (!updated || updated->dirty || updated->can_undo || !updated->can_redo ||
        updated->view.selection_start != 4 || left.GetDocuments()[0].id != second.id) return 6;
    if (left.SetViewState(second.id, {9, 4, 0, 1.0}).status !=
        DocumentWorkspaceStatus::invalid_argument) return 7;
    if (left.Rename(third.id, L"Duplicate", L"C:\\DOCS\\second.txt").status !=
        DocumentWorkspaceStatus::duplicate_path) return 8;

    auto stale = left.PrepareTransfer(third.id, right.GetId());
    if (!stale || !right.AcceptTransfer(*stale) || !right.Find(third.id) ||
        !left.SetDirty(first.id, true) ||
        left.CommitTransfer(*stale).status != DocumentWorkspaceStatus::stale_transfer ||
        !right.RollbackTransfer(*stale) || right.Find(third.id) || !left.Find(third.id)) return 9;
    if (!TransferDocument(left, right, second.id, 0) || left.Find(second.id) ||
        !right.Find(second.id) || right.GetActiveId() != second.id) return 10;

    WorkspaceDocument conflict{{55}, L"Conflict", L"C:\\Docs\\Third.txt"};
    if (!right.Add(conflict)) return 11;
    if (!left.Rename(third.id, L"Third", L"C:\\Docs\\Third.txt")) return 12;
    const auto left_count = left.GetCount();
    if (TransferDocument(left, right, third.id).status !=
            DocumentWorkspaceStatus::duplicate_path || left.GetCount() != left_count ||
        !left.Find(third.id) || right.Find(third.id)) return 13;

    if (!left.Close(first.id) || left.GetRecentlyClosed().size() != 1 ||
        left.GetActiveId() != third.id || !left.ReopenRecentlyClosed() ||
        left.GetActiveId() != first.id || !left.Find(first.id)) return 14;
    if (!left.Close(first.id) || !left.Close(third.id) ||
        left.GetRecentlyClosed().size() != 2 || left.GetRecentlyClosed()[0].id != third.id)
        return 15;
    if (left.ReopenRecentlyClosed(99).status != DocumentWorkspaceStatus::not_found) return 16;

    DocumentWorkspaceModel batch{{80}};
    if (!batch.Add({{801}, L"One"}) || !batch.Add({{802}, L"Two"}) ||
        !batch.Add({{803}, L"Three"}) || !batch.Activate({802})) return 19;
    const std::array duplicate_close{DocumentId{801}, DocumentId{801}};
    if (batch.CloseMany(duplicate_close).status != DocumentWorkspaceStatus::invalid_argument ||
        batch.GetCount() != 3) return 20;
    const std::array missing_close{DocumentId{801}, DocumentId{999}};
    if (batch.CloseMany(missing_close).status != DocumentWorkspaceStatus::not_found ||
        batch.GetCount() != 3) return 21;
    const std::array close_two{DocumentId{801}, DocumentId{802}};
    if (!batch.CloseMany(close_two) || batch.GetCount() != 1 ||
        batch.GetActiveId() != DocumentId{803} || !batch.GetRecentlyClosed().empty()) return 22;

    DocumentWorkspaceModel history{{81}, 2};
    if (!history.Add({{810}, L"Open", L"C:\\Docs\\open.txt"})) return 23;
    const std::array restored_history{
        WorkspaceDocument{{811}, L"Recent one", L"C:\\Docs\\one.txt"},
        WorkspaceDocument{{812}, L"Recent two", L"C:\\Docs\\two.txt"},
        WorkspaceDocument{{813}, L"Truncated", L"C:\\Docs\\three.txt"}};
    if (!history.RestoreRecentlyClosed(restored_history) ||
        history.GetRecentlyClosed().size() != 2 ||
        history.GetRecentlyClosed()[0].id != DocumentId{811}) return 24;
    const std::array invalid_history{
        WorkspaceDocument{{814}, L"Conflict", L"c:/docs/OPEN.txt"}};
    if (history.RestoreRecentlyClosed(invalid_history).status !=
            DocumentWorkspaceStatus::duplicate_path ||
        history.GetRecentlyClosed().size() != 2) return 25;

    DocumentWorkspaceModel rollback_source{{90}};
    DocumentWorkspaceModel rollback_destination{{91}};
    if (!rollback_source.Add({{900}, L"Rollback"})) return 17;
    auto rollback = rollback_source.PrepareTransfer({900}, rollback_destination.GetId());
    if (!rollback || rollback_source.CommitTransfer(*rollback).status !=
                         DocumentWorkspaceStatus::invalid_argument ||
        !rollback_destination.AcceptTransfer(*rollback) ||
        !rollback_destination.SetDirty({900}, true) ||
        rollback_destination.RollbackTransfer(*rollback).status !=
            DocumentWorkspaceStatus::stale_transfer ||
        !rollback_destination.Close({900}, false)) return 18;
    return 0;
}
