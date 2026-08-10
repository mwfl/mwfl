#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace mwtl {

// Stable application identity for a tab. Zero is reserved for "no tab".
struct TabId {
    std::uint64_t value = 0;

    constexpr auto operator<=>(const TabId&) const noexcept = default;
    constexpr explicit operator bool() const noexcept { return value != 0; }
};

struct TabEntry {
    TabId id{};
    std::wstring title;
    bool dirty = false;
    bool closable = true;
};

// UI-thread-owned state model. Returned spans and pointers expire on the next
// mutation. The model never owns page HWNDs or application documents.
class TabWorkspaceModel final {
public:
    bool Add(TabEntry entry);
    bool Remove(TabId id) noexcept;
    bool Move(TabId id, std::size_t new_index) noexcept;
    bool Select(TabId id) noexcept;
    bool SetTitle(TabId id, std::wstring title);
    bool SetDirty(TabId id, bool dirty) noexcept;

    std::span<const TabEntry> GetTabs() const noexcept { return tabs_; }
    std::optional<TabId> GetSelectedId() const noexcept { return selected_; }
    std::optional<std::size_t> FindIndex(TabId id) const noexcept;
    const TabEntry* Find(TabId id) const noexcept;
    bool Empty() const noexcept { return tabs_.empty(); }
    std::size_t GetCount() const noexcept { return tabs_.size(); }

private:
    TabEntry* FindMutable(TabId id) noexcept;

    std::vector<TabEntry> tabs_;
    std::optional<TabId> selected_;
};

}  // namespace mwtl
