#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace mwtl {

struct DocumentId {
    std::uint64_t value = 0;
    constexpr auto operator<=>(const DocumentId&) const noexcept = default;
    constexpr explicit operator bool() const noexcept { return value != 0; }
};

struct WorkspaceId {
    std::uint64_t value = 0;
    constexpr auto operator<=>(const WorkspaceId&) const noexcept = default;
    constexpr explicit operator bool() const noexcept { return value != 0; }
};

struct DocumentViewState {
    std::uint64_t selection_start = 0;
    std::uint64_t selection_end = 0;
    std::uint64_t first_visible_unit = 0;
    double zoom = 1.0;
    bool IsValid() const noexcept;
};

// Logical metadata only. Applications own contents, undo buffers, and page HWNDs
// keyed by id. No application pointer crosses this model boundary.
struct WorkspaceDocument {
    DocumentId id{};
    std::wstring title;
    std::filesystem::path path;
    bool dirty = false;
    bool can_undo = false;
    bool can_redo = false;
    DocumentViewState view{};
};

enum class DocumentWorkspaceStatus {
    success,
    invalid_argument,
    duplicate_id,
    duplicate_path,
    not_found,
    stale_transfer,
};

struct DocumentWorkspaceResult {
    DocumentWorkspaceStatus status = DocumentWorkspaceStatus::invalid_argument;
    std::optional<DocumentId> active;
    explicit operator bool() const noexcept {
        return status == DocumentWorkspaceStatus::success;
    }
};

struct DocumentTransferPlan {
    WorkspaceId source{};
    WorkspaceId destination{};
    WorkspaceDocument document;
    std::size_t source_index = 0;
    std::optional<std::size_t> destination_index;
    bool was_active = false;
    std::uint64_t source_revision = 0;
    std::uint64_t destination_revision = 0;
    bool accepted = false;
    bool IsValid() const noexcept;
};

// UI-thread-owned pure logical model. Returned spans/pointers expire on the next
// mutation. It owns metadata snapshots, never document contents or HWNDs.
class DocumentWorkspaceModel final {
public:
    explicit DocumentWorkspaceModel(WorkspaceId id,
                                    std::size_t maximum_recently_closed = 10);

    DocumentWorkspaceResult Add(WorkspaceDocument document,
                                std::optional<std::size_t> index = std::nullopt);
    DocumentWorkspaceResult Close(DocumentId id, bool remember = true);
    DocumentWorkspaceResult Move(DocumentId id, std::size_t index) noexcept;
    DocumentWorkspaceResult Activate(DocumentId id) noexcept;
    DocumentWorkspaceResult SetDirty(DocumentId id, bool dirty) noexcept;
    DocumentWorkspaceResult SetUndoState(DocumentId id, bool can_undo,
                                         bool can_redo) noexcept;
    DocumentWorkspaceResult SetViewState(DocumentId id,
                                         DocumentViewState state) noexcept;
    DocumentWorkspaceResult Rename(DocumentId id, std::wstring title,
                                   std::filesystem::path path = {});
    DocumentWorkspaceResult ReopenRecentlyClosed(std::size_t index = 0);
    void ClearRecentlyClosed() noexcept { recently_closed_.clear(); }

    std::optional<DocumentTransferPlan> PrepareTransfer(
        DocumentId id, WorkspaceId destination,
        std::optional<std::size_t> destination_index = std::nullopt) const;
    DocumentWorkspaceResult AcceptTransfer(DocumentTransferPlan& plan);
    DocumentWorkspaceResult CommitTransfer(const DocumentTransferPlan& plan);
    DocumentWorkspaceResult RollbackTransfer(const DocumentTransferPlan& plan);

    WorkspaceId GetId() const noexcept { return id_; }
    std::span<const WorkspaceDocument> GetDocuments() const noexcept { return documents_; }
    std::span<const WorkspaceDocument> GetRecentlyClosed() const noexcept {
        return recently_closed_;
    }
    std::optional<DocumentId> GetActiveId() const noexcept { return active_; }
    const WorkspaceDocument* Find(DocumentId id) const noexcept;
    std::optional<std::size_t> FindIndex(DocumentId id) const noexcept;
    bool ContainsPath(const std::filesystem::path& path) const;
    std::size_t GetCount() const noexcept { return documents_.size(); }
    std::uint64_t GetRevision() const noexcept { return revision_; }

private:
    WorkspaceDocument* FindMutable(DocumentId id) noexcept;
    bool IsValidDocument(const WorkspaceDocument& document) const noexcept;
    void RememberClosed(const WorkspaceDocument& document);
    DocumentWorkspaceResult Success() const noexcept;

    WorkspaceId id_{};
    std::size_t maximum_recently_closed_ = 0;
    std::vector<WorkspaceDocument> documents_;
    std::vector<WorkspaceDocument> recently_closed_;
    std::optional<DocumentId> active_;
    std::uint64_t revision_ = 0;
};

// All-or-rollback metadata transfer. Source removal occurs only after destination
// adoption. A failed source commit removes the destination copy. Applications
// transfer their content owner only after this function succeeds.
DocumentWorkspaceResult TransferDocument(
    DocumentWorkspaceModel& source, DocumentWorkspaceModel& destination,
    DocumentId id, std::optional<std::size_t> destination_index = std::nullopt);

}  // namespace mwtl
