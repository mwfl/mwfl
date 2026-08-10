#pragma once

#include <cstddef>
#include <exception>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <mwfl/document_workspace.h>
#include <mwfl/text_file.h>

namespace mwfl {

inline constexpr std::uint32_t document_session_schema_version = 1;

struct SessionDocument {
    WorkspaceDocument metadata;
    std::optional<FileStamp> stamp;
};

struct WorkspaceSession {
    WorkspaceId workspace{};
    std::optional<DocumentId> active;
    std::vector<SessionDocument> documents;
    std::vector<SessionDocument> recently_closed;
};

struct DocumentSession {
    std::uint32_t schema_version = document_session_schema_version;
    std::vector<WorkspaceSession> workspaces;
};

struct DocumentSessionLimits {
    std::size_t maximum_bytes = 4 * 1024 * 1024;
    std::size_t maximum_workspaces = 32;
    std::size_t maximum_documents_per_workspace = 4096;
    std::size_t maximum_string_units = 32 * 1024;
};

enum class DocumentSessionStatus {
    success, not_found, unsupported_version, malformed, oversized,
    duplicate_identity, io_error,
};

struct DocumentSessionResult {
    DocumentSessionStatus status = DocumentSessionStatus::malformed;
    std::optional<DocumentSession> session = std::nullopt;
    DWORD native_error = ERROR_SUCCESS;
    explicit operator bool() const noexcept {
        return status == DocumentSessionStatus::success && session.has_value();
    }
};

WorkspaceSession CaptureWorkspaceSession(const DocumentWorkspaceModel& workspace);
std::wstring SerializeDocumentSession(const DocumentSession& session);
DocumentSessionResult ParseDocumentSession(
    std::wstring_view text, const DocumentSessionLimits& limits = {});
DocumentSessionResult LoadDocumentSession(
    const std::filesystem::path& path, const DocumentSessionLimits& limits = {});
DocumentSessionStatus SaveDocumentSessionAtomic(
    const std::filesystem::path& path, const DocumentSession& session,
    const DocumentSessionLimits& limits = {});

enum class SessionDocumentDisposition {
    restore, missing, changed, untrusted, rejected,
};

using SessionDocumentValidator =
    std::function<SessionDocumentDisposition(const SessionDocument&)>;
using SessionDocumentRestoreHandler = std::function<bool(const SessionDocument&)>;

struct SessionRestoreIssue {
    DocumentId document{};
    SessionDocumentDisposition disposition = SessionDocumentDisposition::rejected;
    bool recently_closed = false;
};

enum class SessionRestoreStatus {
    success, partial_success, invalid_argument, target_not_empty,
};

struct SessionRestoreResult {
    SessionRestoreStatus status = SessionRestoreStatus::invalid_argument;
    std::size_t restored_documents = 0;
    std::size_t restored_recently_closed = 0;
    std::vector<SessionRestoreIssue> issues;
    std::exception_ptr error{};
    explicit operator bool() const noexcept {
        return status == SessionRestoreStatus::success ||
               status == SessionRestoreStatus::partial_success;
    }
};

// Restores into an empty, matching workspace. Validation and application
// restoration callbacks run before each model mutation and exceptions are
// contained. Missing, changed, or untrusted paths are skipped incrementally.
// Callbacks must not mutate target; a revision change rejects that entry.
SessionRestoreResult RestoreWorkspaceSession(
    DocumentWorkspaceModel& target, const WorkspaceSession& session,
    const SessionDocumentValidator& validate,
    const SessionDocumentRestoreHandler& restore);

}  // namespace mwfl
