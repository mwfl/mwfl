#include <mwtl/document_coordination.h>

#include <algorithm>
#include <array>
#include <ranges>
#include <utility>

namespace mwtl {

bool ActiveDocumentCommandIds::IsValid() const noexcept {
    const std::array values{save.value, close.value, undo.value, redo.value};
    if (std::ranges::any_of(values, [](int value) { return value == 0; })) return false;
    for (std::size_t left = 0; left < values.size(); ++left)
        for (std::size_t right = left + 1; right < values.size(); ++right)
            if (values[left] == values[right]) return false;
    return true;
}

ActiveDocumentCommandProjection BuildActiveDocumentCommandProjection(
    const DocumentWorkspaceModel& workspace) {
    ActiveDocumentCommandProjection result;
    result.document = workspace.GetActiveId();
    if (!result.document) return result;
    const auto* document = workspace.Find(*result.document);
    if (!document) { result.document.reset(); return result; }
    result.save_text = L"Save " + document->title;
    result.close_text = L"Close " + document->title;
    result.status_text = document->title;
    if (document->dirty) result.status_text += L" — modified";
    result.status_text += L" — " + std::to_wstring(workspace.GetCount()) +
                          L" document(s)";
    result.can_save = document->dirty;
    result.can_close = true;
    result.can_undo = document->can_undo;
    result.can_redo = document->can_redo;
    return result;
}

DocumentCommandProjectionStatus ApplyActiveDocumentCommandProjection(
    CommandSet& commands, const ActiveDocumentCommandIds& ids,
    const ActiveDocumentCommandProjection& projection) {
    if (!ids.IsValid()) return DocumentCommandProjectionStatus::invalid_argument;
    auto* save = commands.Find(ids.save);
    auto* close = commands.Find(ids.close);
    auto* undo = commands.Find(ids.undo);
    auto* redo = commands.Find(ids.redo);
    if (!save || !close || !undo || !redo)
        return DocumentCommandProjectionStatus::command_not_found;
    save->SetText(projection.save_text).SetEnabled(projection.can_save);
    close->SetText(projection.close_text).SetEnabled(projection.can_close);
    undo->SetEnabled(projection.can_undo);
    redo->SetEnabled(projection.can_redo);
    return DocumentCommandProjectionStatus::success;
}

ActiveDocumentRouteResult RouteActiveDocument(
    const DocumentWorkspaceModel& workspace,
    const ActiveDocumentHandler& handler) noexcept {
    if (!handler)
        return {ActiveDocumentRouteStatus::invalid_argument, {}, {}};
    const auto active = workspace.GetActiveId();
    if (!active)
        return {ActiveDocumentRouteStatus::no_active_document, active, {}};
    try {
        handler(*active);
    } catch (...) {
        return {ActiveDocumentRouteStatus::callback_failure, active,
                std::current_exception()};
    }
    if (workspace.GetActiveId() != active)
        return {ActiveDocumentRouteStatus::active_document_changed, active, {}};
    return {ActiveDocumentRouteStatus::success, active, {}};
}

CoordinatedClosePlan BuildCoordinatedClosePlan(
    const DocumentWorkspaceModel& workspace,
    std::span<const DocumentCloseDecision> decisions) {
    CoordinatedClosePlan plan;
    plan.workspace_revision = workspace.GetRevision();
    for (std::size_t index = 0; index < decisions.size(); ++index) {
        const auto* decided = workspace.Find(decisions[index].document);
        if (!decisions[index].document || !decided || !decided->dirty) {
            plan.status = CoordinatedCloseStatus::invalid_argument;
            return plan;
        }
        for (std::size_t other = index + 1; other < decisions.size(); ++other) {
            if (decisions[index].document == decisions[other].document) {
                plan.status = CoordinatedCloseStatus::invalid_argument;
                return plan;
            }
        }
    }
    for (const auto& document : workspace.GetDocuments()) {
        plan.close_documents.push_back(document.id);
        if (!document.dirty) continue;
        const auto found = std::ranges::find(decisions, document.id,
                                             &DocumentCloseDecision::document);
        if (found == decisions.end()) {
            plan.status = CoordinatedCloseStatus::incomplete_decisions;
            return plan;
        }
        if (found->choice == DocumentCloseChoice::cancel) {
            plan.status = CoordinatedCloseStatus::cancelled;
            return plan;
        }
        if (found->choice == DocumentCloseChoice::save)
            plan.save_before_close.push_back(document.id);
    }
    plan.status = CoordinatedCloseStatus::ready;
    return plan;
}

CoordinatedCloseResult CommitCoordinatedCloseAfterSaves(
    DocumentWorkspaceModel& workspace, const CoordinatedClosePlan& plan) noexcept {
    if (!plan)
        return {CoordinatedCloseStatus::invalid_argument, {}, {}};
    if (workspace.GetRevision() != plan.workspace_revision)
        return {CoordinatedCloseStatus::stale_workspace, {}, {}};
    if (plan.close_documents.empty())
        return {CoordinatedCloseStatus::success, {}, {}};
    try {
        const auto closed = workspace.CloseMany(plan.close_documents);
        if (!closed)
            return {CoordinatedCloseStatus::commit_failure, {}, {}};
    } catch (...) {
        return {CoordinatedCloseStatus::commit_failure, {},
                std::current_exception()};
    }
    return {CoordinatedCloseStatus::success, {}, {}};
}

CoordinatedCloseResult ExecuteCoordinatedClose(
    DocumentWorkspaceModel& workspace, const CoordinatedClosePlan& plan,
    const DocumentSaveHandler& save) noexcept {
    if (!plan)
        return {CoordinatedCloseStatus::invalid_argument, {}, {}};
    if (workspace.GetRevision() != plan.workspace_revision)
        return {CoordinatedCloseStatus::stale_workspace, {}, {}};
    if (plan.close_documents.empty())
        return {CoordinatedCloseStatus::success, {}, {}};
    if (!plan.save_before_close.empty() && !save)
        return {CoordinatedCloseStatus::invalid_argument,
                plan.save_before_close.front(), {}};
    for (const auto id : plan.save_before_close) {
        try {
            if (!save(id)) return {CoordinatedCloseStatus::save_failed, id, {}};
        } catch (...) {
            return {CoordinatedCloseStatus::callback_failure, id,
                    std::current_exception()};
        }
        if (workspace.GetRevision() != plan.workspace_revision)
            return {CoordinatedCloseStatus::stale_workspace, id, {}};
    }
    return CommitCoordinatedCloseAfterSaves(workspace, plan);
}

}  // namespace mwtl
