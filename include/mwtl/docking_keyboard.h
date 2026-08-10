#pragma once

#include <optional>
#include <span>
#include <string>
#include <vector>

#include <mwtl/docking_drag.h>

namespace mwtl {

enum class DockKeyboardMove { left, up, right, down, next, previous };

struct DockKeyboardSelection {
    DockDropTarget target{};
    std::wstring announcement;
};

std::wstring DescribeDockTarget(const DockDropTarget& target);

// Copies a bounded target list at Begin so native target windows may disappear
// without leaving borrowed spans. Directional movement uses target centers;
// next/previous preserves the application-supplied accessible reading order.
class DockKeyboardSession final {
public:
    bool Begin(DockPanelId panel, std::span<const DockDropTarget> targets,
               std::optional<std::uint64_t> preferred_target = std::nullopt);
    bool Move(DockKeyboardMove direction) noexcept;
    std::optional<DockKeyboardSelection> Accept() noexcept;
    void Cancel() noexcept;
    void Reset() noexcept;

    bool IsActive() const noexcept { return active_; }
    DockPanelId GetPanel() const noexcept { return panel_; }
    std::optional<DockKeyboardSelection> GetSelection() const;

private:
    bool SelectIndex(std::size_t index) noexcept;

    DockPanelId panel_{};
    std::vector<DockDropTarget> targets_;
    std::optional<std::size_t> selected_;
    bool active_ = false;
};

}  // namespace mwtl
