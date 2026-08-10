#include <mwtl/tab_workspace.h>

#include <algorithm>
#include <ranges>
#include <utility>

namespace mwtl {

std::optional<std::size_t> TabWorkspaceModel::FindIndex(TabId id) const noexcept {
    if (!id) return std::nullopt;
    const auto found = std::ranges::find(tabs_, id, &TabEntry::id);
    if (found == tabs_.end()) return std::nullopt;
    return static_cast<std::size_t>(found - tabs_.begin());
}

const TabEntry* TabWorkspaceModel::Find(TabId id) const noexcept {
    const auto index = FindIndex(id);
    return index ? &tabs_[*index] : nullptr;
}

TabEntry* TabWorkspaceModel::FindMutable(TabId id) noexcept {
    const auto index = FindIndex(id);
    return index ? &tabs_[*index] : nullptr;
}

bool TabWorkspaceModel::Add(TabEntry entry) {
    if (!entry.id || entry.title.empty() || Find(entry.id) != nullptr) return false;
    tabs_.push_back(std::move(entry));
    if (!selected_) selected_ = tabs_.back().id;
    return true;
}

bool TabWorkspaceModel::Remove(TabId id) noexcept {
    const auto index = FindIndex(id);
    if (!index) return false;
    const bool removed_selection = selected_ == id;
    tabs_.erase(tabs_.begin() + static_cast<std::ptrdiff_t>(*index));
    if (removed_selection) {
        if (tabs_.empty()) {
            selected_.reset();
        } else {
            selected_ = tabs_[std::min(*index, tabs_.size() - 1)].id;
        }
    }
    return true;
}

bool TabWorkspaceModel::Move(TabId id, std::size_t new_index) noexcept {
    const auto old_index = FindIndex(id);
    if (!old_index || new_index >= tabs_.size()) return false;
    if (*old_index == new_index) return true;
    TabEntry moved = std::move(tabs_[*old_index]);
    tabs_.erase(tabs_.begin() + static_cast<std::ptrdiff_t>(*old_index));
    tabs_.insert(tabs_.begin() + static_cast<std::ptrdiff_t>(new_index), std::move(moved));
    return true;
}

bool TabWorkspaceModel::Select(TabId id) noexcept {
    if (Find(id) == nullptr) return false;
    selected_ = id;
    return true;
}

bool TabWorkspaceModel::SetTitle(TabId id, std::wstring title) {
    if (title.empty()) return false;
    auto* entry = FindMutable(id);
    if (entry == nullptr) return false;
    entry->title = std::move(title);
    return true;
}

bool TabWorkspaceModel::SetDirty(TabId id, bool dirty) noexcept {
    auto* entry = FindMutable(id);
    if (entry == nullptr) return false;
    entry->dirty = dirty;
    return true;
}

}  // namespace mwtl
