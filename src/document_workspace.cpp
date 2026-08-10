#include <mwtl/document_workspace.h>

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <ranges>
#include <stdexcept>
#include <utility>

namespace mwtl {
namespace {
bool SamePath(const std::filesystem::path& left,
              const std::filesystem::path& right) {
    if (left.empty() || right.empty()) return false;
    auto normalized_left = left.lexically_normal();
    auto normalized_right = right.lexically_normal();
    normalized_left.make_preferred();
    normalized_right.make_preferred();
    const auto& a = normalized_left.native();
    const auto& b = normalized_right.native();
    return ::CompareStringOrdinal(a.c_str(), static_cast<int>(a.size()),
                                  b.c_str(), static_cast<int>(b.size()), TRUE) == CSTR_EQUAL;
}
}  // namespace

bool DocumentViewState::IsValid() const noexcept {
    return selection_start <= selection_end && std::isfinite(zoom) &&
           zoom >= 0.1 && zoom <= 10.0;
}

bool DocumentTransferPlan::IsValid() const noexcept {
    return source && destination && source != destination && document.id &&
           !document.title.empty() && document.view.IsValid();
}

DocumentWorkspaceModel::DocumentWorkspaceModel(
    WorkspaceId id, std::size_t maximum_recently_closed)
    : id_(id), maximum_recently_closed_(maximum_recently_closed) {
    if (!id_) throw std::invalid_argument("workspace ID must be nonzero");
}

std::optional<std::size_t> DocumentWorkspaceModel::FindIndex(DocumentId id) const noexcept {
    if (!id) return std::nullopt;
    const auto found = std::ranges::find(documents_, id, &WorkspaceDocument::id);
    if (found == documents_.end()) return std::nullopt;
    return static_cast<std::size_t>(found - documents_.begin());
}

const WorkspaceDocument* DocumentWorkspaceModel::Find(DocumentId id) const noexcept {
    const auto index = FindIndex(id);
    return index ? &documents_[*index] : nullptr;
}

WorkspaceDocument* DocumentWorkspaceModel::FindMutable(DocumentId id) noexcept {
    const auto index = FindIndex(id);
    return index ? &documents_[*index] : nullptr;
}

bool DocumentWorkspaceModel::ContainsPath(const std::filesystem::path& path) const {
    return !path.empty() && std::ranges::any_of(
        documents_, [&](const WorkspaceDocument& item) { return SamePath(item.path, path); });
}

bool DocumentWorkspaceModel::IsValidDocument(
    const WorkspaceDocument& document) const noexcept {
    return document.id && !document.title.empty() && document.view.IsValid();
}

DocumentWorkspaceResult DocumentWorkspaceModel::Success() const noexcept {
    return {DocumentWorkspaceStatus::success, active_};
}

DocumentWorkspaceResult DocumentWorkspaceModel::Add(
    WorkspaceDocument document, std::optional<std::size_t> index) {
    if (!IsValidDocument(document) || (index && *index > documents_.size()))
        return {DocumentWorkspaceStatus::invalid_argument, active_};
    if (Find(document.id)) return {DocumentWorkspaceStatus::duplicate_id, active_};
    if (ContainsPath(document.path)) return {DocumentWorkspaceStatus::duplicate_path, active_};
    const std::size_t insertion = index.value_or(documents_.size());
    documents_.insert(documents_.begin() + static_cast<std::ptrdiff_t>(insertion),
                      std::move(document));
    if (!active_) active_ = documents_[insertion].id;
    ++revision_;
    return Success();
}

void DocumentWorkspaceModel::RememberClosed(const WorkspaceDocument& document) {
    if (maximum_recently_closed_ == 0) return;
    recently_closed_.erase(std::remove_if(recently_closed_.begin(), recently_closed_.end(),
        [&](const WorkspaceDocument& item) {
            return item.id == document.id || SamePath(item.path, document.path);
        }), recently_closed_.end());
    recently_closed_.insert(recently_closed_.begin(), document);
    if (recently_closed_.size() > maximum_recently_closed_)
        recently_closed_.resize(maximum_recently_closed_);
}

DocumentWorkspaceResult DocumentWorkspaceModel::Close(DocumentId id, bool remember) {
    const auto index = FindIndex(id);
    if (!index) return {DocumentWorkspaceStatus::not_found, active_};
    if (remember) RememberClosed(documents_[*index]);
    const bool was_active = active_ == id;
    documents_.erase(documents_.begin() + static_cast<std::ptrdiff_t>(*index));
    if (was_active) {
        if (documents_.empty()) active_.reset();
        else active_ = documents_[(std::min)(*index, documents_.size() - 1)].id;
    }
    ++revision_;
    return Success();
}

DocumentWorkspaceResult DocumentWorkspaceModel::CloseMany(
    std::span<const DocumentId> ids) {
    if (ids.empty()) return {DocumentWorkspaceStatus::invalid_argument, active_};
    std::vector<DocumentId> unique;
    unique.reserve(ids.size());
    for (const auto id : ids) {
        if (!id || std::ranges::find(unique, id) != unique.end())
            return {DocumentWorkspaceStatus::invalid_argument, active_};
        if (!Find(id)) return {DocumentWorkspaceStatus::not_found, active_};
        unique.push_back(id);
    }
    const auto closing = [&](DocumentId id) {
        return std::ranges::find(unique, id) != unique.end();
    };
    std::vector<WorkspaceDocument> remaining;
    remaining.reserve(documents_.size() - unique.size());
    for (const auto& document : documents_)
        if (!closing(document.id)) remaining.push_back(document);

    std::optional<DocumentId> next_active = active_;
    if (next_active && closing(*next_active)) {
        next_active.reset();
        const auto old_active = FindIndex(*active_).value_or(0);
        for (std::size_t index = old_active; index < documents_.size(); ++index) {
            if (!closing(documents_[index].id)) { next_active = documents_[index].id; break; }
        }
        if (!next_active) {
            for (std::size_t index = old_active; index > 0; --index) {
                if (!closing(documents_[index - 1].id)) {
                    next_active = documents_[index - 1].id;
                    break;
                }
            }
        }
    }
    documents_.swap(remaining);
    active_ = next_active;
    ++revision_;
    return Success();
}

DocumentWorkspaceResult DocumentWorkspaceModel::Move(DocumentId id,
                                                       std::size_t index) noexcept {
    const auto current = FindIndex(id);
    if (!current) return {DocumentWorkspaceStatus::not_found, active_};
    if (index >= documents_.size())
        return {DocumentWorkspaceStatus::invalid_argument, active_};
    if (*current == index) return Success();
    auto document = std::move(documents_[*current]);
    documents_.erase(documents_.begin() + static_cast<std::ptrdiff_t>(*current));
    documents_.insert(documents_.begin() + static_cast<std::ptrdiff_t>(index),
                      std::move(document));
    ++revision_;
    return Success();
}

DocumentWorkspaceResult DocumentWorkspaceModel::Activate(DocumentId id) noexcept {
    if (!Find(id)) return {DocumentWorkspaceStatus::not_found, active_};
    if (active_ != id) { active_ = id; ++revision_; }
    return Success();
}

DocumentWorkspaceResult DocumentWorkspaceModel::SetDirty(DocumentId id, bool dirty) noexcept {
    auto* document = FindMutable(id);
    if (!document) return {DocumentWorkspaceStatus::not_found, active_};
    if (document->dirty != dirty) { document->dirty = dirty; ++revision_; }
    return Success();
}

DocumentWorkspaceResult DocumentWorkspaceModel::SetUndoState(
    DocumentId id, bool can_undo, bool can_redo) noexcept {
    auto* document = FindMutable(id);
    if (!document) return {DocumentWorkspaceStatus::not_found, active_};
    if (document->can_undo != can_undo || document->can_redo != can_redo) {
        document->can_undo = can_undo;
        document->can_redo = can_redo;
        ++revision_;
    }
    return Success();
}

DocumentWorkspaceResult DocumentWorkspaceModel::SetViewState(
    DocumentId id, DocumentViewState state) noexcept {
    if (!state.IsValid()) return {DocumentWorkspaceStatus::invalid_argument, active_};
    auto* document = FindMutable(id);
    if (!document) return {DocumentWorkspaceStatus::not_found, active_};
    document->view = state;
    ++revision_;
    return Success();
}

DocumentWorkspaceResult DocumentWorkspaceModel::Rename(
    DocumentId id, std::wstring title, std::filesystem::path path) {
    if (title.empty()) return {DocumentWorkspaceStatus::invalid_argument, active_};
    auto* document = FindMutable(id);
    if (!document) return {DocumentWorkspaceStatus::not_found, active_};
    if (!path.empty() && std::ranges::any_of(documents_, [&](const WorkspaceDocument& item) {
            return item.id != id && SamePath(item.path, path);
        })) return {DocumentWorkspaceStatus::duplicate_path, active_};
    document->title = std::move(title);
    document->path = std::move(path);
    ++revision_;
    return Success();
}

DocumentWorkspaceResult DocumentWorkspaceModel::ReopenRecentlyClosed(std::size_t index) {
    if (index >= recently_closed_.size())
        return {DocumentWorkspaceStatus::not_found, active_};
    WorkspaceDocument document = recently_closed_[index];
    document.dirty = false;
    const auto added = Add(document);
    if (!added) return added;
    recently_closed_.erase(recently_closed_.begin() + static_cast<std::ptrdiff_t>(index));
    active_ = document.id;
    ++revision_;
    return Success();
}

std::optional<DocumentTransferPlan> DocumentWorkspaceModel::PrepareTransfer(
    DocumentId id, WorkspaceId destination,
    std::optional<std::size_t> destination_index) const {
    const auto source_index = FindIndex(id);
    if (!source_index || !destination || destination == id_) return std::nullopt;
    return DocumentTransferPlan{id_, destination, documents_[*source_index], *source_index,
                                destination_index, active_ == id, revision_, 0, false};
}

DocumentWorkspaceResult DocumentWorkspaceModel::AcceptTransfer(
    DocumentTransferPlan& plan) {
    if (!plan.IsValid() || plan.destination != id_ || plan.accepted)
        return {DocumentWorkspaceStatus::invalid_argument, active_};
    const auto added = Add(plan.document, plan.destination_index);
    if (added) {
        plan.accepted = true;
        plan.destination_revision = revision_;
    }
    return added;
}

DocumentWorkspaceResult DocumentWorkspaceModel::CommitTransfer(
    const DocumentTransferPlan& plan) {
    if (!plan.IsValid() || plan.source != id_ || !plan.accepted)
        return {DocumentWorkspaceStatus::invalid_argument, active_};
    if (revision_ != plan.source_revision || !Find(plan.document.id))
        return {DocumentWorkspaceStatus::stale_transfer, active_};
    return Close(plan.document.id, false);
}

DocumentWorkspaceResult DocumentWorkspaceModel::RollbackTransfer(
    const DocumentTransferPlan& plan) {
    if (!plan.IsValid() || plan.destination != id_ || !plan.accepted)
        return {DocumentWorkspaceStatus::invalid_argument, active_};
    if (revision_ != plan.destination_revision || !Find(plan.document.id))
        return {DocumentWorkspaceStatus::stale_transfer, active_};
    return Close(plan.document.id, false);
}

DocumentWorkspaceResult TransferDocument(
    DocumentWorkspaceModel& source, DocumentWorkspaceModel& destination,
    DocumentId id, std::optional<std::size_t> destination_index) {
    auto plan = source.PrepareTransfer(id, destination.GetId(), destination_index);
    if (!plan) return {DocumentWorkspaceStatus::invalid_argument, source.GetActiveId()};
    const auto accepted = destination.AcceptTransfer(*plan);
    if (!accepted) return accepted;
    const auto committed = source.CommitTransfer(*plan);
    if (!committed) {
        destination.RollbackTransfer(*plan);
        return committed;
    }
    if (plan->was_active) destination.Activate(id);
    return {DocumentWorkspaceStatus::success, destination.GetActiveId()};
}

}  // namespace mwtl
