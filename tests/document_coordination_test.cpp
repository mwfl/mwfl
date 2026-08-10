#include <mwtl/document_coordination.h>

#include <array>
#include <stdexcept>

int main() {
    using namespace mwtl;
    DocumentWorkspaceModel workspace{{1}};
    if (!workspace.Add({{11}, L"One", {}, true, true, false}) ||
        !workspace.Add({{22}, L"Two", {}, false, false, true})) return 1;

    CommandSet commands;
    commands.Add(Command{{101}, L"Save"}).Add(Command{{102}, L"Close"})
            .Add(Command{{103}, L"Undo"}).Add(Command{{104}, L"Redo"});
    const ActiveDocumentCommandIds ids{{101}, {102}, {103}, {104}};
    auto projection = BuildActiveDocumentCommandProjection(workspace);
    if (projection.document != DocumentId{11} || !projection.can_save ||
        !projection.can_undo || projection.can_redo ||
        ApplyActiveDocumentCommandProjection(commands, ids, projection) !=
            DocumentCommandProjectionStatus::success ||
        !commands.Find({101})->IsEnabled() || !commands.Find({103})->IsEnabled() ||
        commands.Find({104})->IsEnabled()) return 2;
    if (ApplyActiveDocumentCommandProjection(commands, {{101}, {101}, {103}, {104}}, projection) !=
        DocumentCommandProjectionStatus::invalid_argument) return 3;

    DocumentId routed{};
    if (!RouteActiveDocument(workspace, [&](DocumentId id) { routed = id; }) ||
        routed != DocumentId{11}) return 4;
    const auto reentrant = RouteActiveDocument(workspace, [&](DocumentId) {
        workspace.Activate({22});
    });
    if (reentrant.status != ActiveDocumentRouteStatus::active_document_changed ||
        reentrant.routed_document != DocumentId{11}) return 5;
    const auto failure = RouteActiveDocument(workspace, [](DocumentId) { throw 7; });
    if (failure.status != ActiveDocumentRouteStatus::callback_failure || !failure.error) return 6;
    if (RouteActiveDocument(workspace, {}).status !=
        ActiveDocumentRouteStatus::invalid_argument) return 18;

    workspace.Activate({11});
    auto incomplete = BuildCoordinatedClosePlan(workspace, {});
    if (incomplete.status != CoordinatedCloseStatus::incomplete_decisions) return 7;
    const std::array cancel{DocumentCloseDecision{{11}, DocumentCloseChoice::cancel}};
    if (BuildCoordinatedClosePlan(workspace, cancel).status !=
        CoordinatedCloseStatus::cancelled) return 8;
    const std::array clean_decision{DocumentCloseDecision{{22}, DocumentCloseChoice::discard}};
    if (BuildCoordinatedClosePlan(workspace, clean_decision).status !=
        CoordinatedCloseStatus::invalid_argument) return 16;
    const std::array duplicate_decision{
        DocumentCloseDecision{{11}, DocumentCloseChoice::save},
        DocumentCloseDecision{{11}, DocumentCloseChoice::discard}};
    if (BuildCoordinatedClosePlan(workspace, duplicate_decision).status !=
        CoordinatedCloseStatus::invalid_argument) return 17;
    const std::array save{DocumentCloseDecision{{11}, DocumentCloseChoice::save}};
    auto plan = BuildCoordinatedClosePlan(workspace, save);
    if (!plan || plan.save_before_close.size() != 1 || plan.close_documents.size() != 2)
        return 9;
    const auto failed = ExecuteCoordinatedClose(workspace, plan,
        [](DocumentId) { return false; });
    if (failed.status != CoordinatedCloseStatus::save_failed || workspace.GetCount() != 2)
        return 10;
    const auto thrown = ExecuteCoordinatedClose(workspace, plan,
        [](DocumentId) -> bool { throw std::runtime_error("save"); });
    if (thrown.status != CoordinatedCloseStatus::callback_failure || !thrown.error ||
        workspace.GetCount() != 2) return 11;
    const auto changed = ExecuteCoordinatedClose(workspace, plan, [&](DocumentId) {
        workspace.SetUndoState({22}, true, false);
        return true;
    });
    if (changed.status != CoordinatedCloseStatus::stale_workspace ||
        workspace.GetCount() != 2) return 12;

    plan = BuildCoordinatedClosePlan(workspace, save);
    if (!ExecuteCoordinatedClose(workspace, plan, [](DocumentId) { return true; }) ||
        workspace.GetCount() != 0 || workspace.GetActiveId()) return 13;

    DocumentWorkspaceModel clean{{2}};
    if (!clean.Add({{31}, L"Clean"})) return 14;
    const auto clean_plan = BuildCoordinatedClosePlan(clean, {});
    if (!clean_plan || !ExecuteCoordinatedClose(clean, clean_plan, {}) || clean.GetCount() != 0)
        return 15;
    const auto empty_plan = BuildCoordinatedClosePlan(clean, {});
    if (!empty_plan || !ExecuteCoordinatedClose(clean, empty_plan, {})) return 19;
    return 0;
}
