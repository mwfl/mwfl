#pragma once

#include <windows.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace hot_corners {

enum class Corner : std::uint8_t { top_left, top_right, bottom_left, bottom_right };
enum class Action : std::uint8_t { none, task_view, notifications, start, show_desktop };

struct MonitorActions {
    std::array<Action, 4> corners{
        Action::task_view, Action::notifications, Action::start, Action::show_desktop};
};

struct Settings {
    std::uint32_t dwell_ms = 350;
    LONG tolerance = 2;
    bool enabled = true;
    bool pause_for_fullscreen = true;
    std::vector<MonitorActions> monitors;
};

struct Hit {
    std::size_t monitor = 0;
    Corner corner = Corner::top_left;
    constexpr bool operator==(const Hit&) const noexcept = default;
};

std::array<bool, 4> ExposedCorners(
    std::span<const RECT> monitors, std::size_t monitor) noexcept;
std::optional<Hit> Detect(
    POINT cursor, std::span<const RECT> monitors, LONG tolerance) noexcept;
Action ResolveAction(const Settings& settings, Hit hit) noexcept;

class DwellTracker final {
public:
    std::optional<Hit> Update(
        std::optional<Hit> hit, std::uint64_t now_ms,
        std::uint64_t dwell_ms) noexcept;
    void Reset() noexcept;

private:
    std::optional<Hit> current_;
    std::uint64_t entered_at_ = 0;
    bool fired_ = false;
};

}  // namespace hot_corners
