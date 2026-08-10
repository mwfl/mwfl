#pragma once

#include <exception>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <mwfl/command.h>
#include <mwfl/document_workspace.h>

namespace mwfl {

struct ActiveDocumentCommandIds {
    ControlId save{};
    ControlId close{};
    ControlId undo{};
    ControlId redo{};
    bool IsValid() const noexcept;
};

struct ActiveDocumentCommandProjection {
    std::optional<DocumentId> document;
    std::wstring save_text = L"Save";
    std::wstring close_text = L"Close";
    std::wstring status_text = L"No active document";
    bool can_save = false;
    bool can_close = false;
    bool can_undo = false;
    bool can_redo = false;
};

ActiveDocumentCommandProjection BuildActiveDocumentCommandProjection(
    const DocumentWorkspaceModel& workspace);

enum class DocumentCommandProjectionStatus {
    success,
    invalid_argument,
    command_not_found,
};

// Updates text/enabled presentation only. Existing checked state, shortcut,
// visibility, image, and handlers remain application-owned and should use
// RouteActiveDocument at invocation time rather than capture a document pointer.
DocumentCommandProjectionStatus ApplyActiveDocumentCommandProjection(
    CommandSet& commands, const ActiveDocumentCommandIds& ids,
    const ActiveDocumentCommandProjection& projection);

enum class ActiveDocumentRouteStatus {
    success,
    invalid_argument,
    no_active_document,
    active_document_changed,
    callback_failure,
};

struct ActiveDocumentRouteResult {
    ActiveDocumentRouteStatus status = ActiveDocumentRouteStatus::no_active_document;
    std::optional<DocumentId> routed_document = std::nullopt;
    std::exception_ptr error{};
    explicit operator bool() const noexcept {
        return status == ActiveDocumentRouteStatus::success;
    }
};

using ActiveDocumentHandler = std::function<void(DocumentId)>;

// Snapshots the active ID, invokes without locks, contains exceptions, then
// reports reentrant activation instead of silently using a new global current.
ActiveDocumentRouteResult RouteActiveDocument(
    const DocumentWorkspaceModel& workspace,
    const ActiveDocumentHandler& handler) noexcept;

enum class DocumentCloseChoice { save, discard, cancel };

struct DocumentCloseDecision {
    DocumentId document{};
    DocumentCloseChoice choice = DocumentCloseChoice::cancel;
};

enum class CoordinatedCloseStatus {
    ready,
    success,
    incomplete_decisions,
    cancelled,
    invalid_argument,
    stale_workspace,
    save_failed,
    callback_failure,
    commit_failure,
};

struct CoordinatedClosePlan {
    CoordinatedCloseStatus status = CoordinatedCloseStatus::invalid_argument;
    std::uint64_t workspace_revision = 0;
    std::vector<DocumentId> save_before_close;
    std::vector<DocumentId> close_documents;
    explicit operator bool() const noexcept {
        return status == CoordinatedCloseStatus::ready;
    }
};

// Decisions are required only for dirty documents. The plan never invokes I/O
// and never mutates the workspace.
CoordinatedClosePlan BuildCoordinatedClosePlan(
    const DocumentWorkspaceModel& workspace,
    std::span<const DocumentCloseDecision> decisions);

using DocumentSaveHandler = std::function<bool(DocumentId)>;

struct CoordinatedCloseResult {
    CoordinatedCloseStatus status = CoordinatedCloseStatus::invalid_argument;
    std::optional<DocumentId> failed_document = std::nullopt;
    std::exception_ptr error{};
    explicit operator bool() const noexcept {
        return status == CoordinatedCloseStatus::success;
    }
};

// Commit phase for applications that complete plan.save_before_close through
// asynchronous workflows. The application must call this only after every
// requested save succeeds. A changed revision rejects the stale plan.
CoordinatedCloseResult CommitCoordinatedCloseAfterSaves(
    DocumentWorkspaceModel& workspace,
    const CoordinatedClosePlan& plan) noexcept;

// Runs every save before one CloseMany commit. Callback failure, cancellation,
// exception, or any reentrant workspace mutation leaves all documents open.
// Closing an already-empty workspace is a successful no-op.
CoordinatedCloseResult ExecuteCoordinatedClose(
    DocumentWorkspaceModel& workspace, const CoordinatedClosePlan& plan,
    const DocumentSaveHandler& save) noexcept;

}  // namespace mwfl
