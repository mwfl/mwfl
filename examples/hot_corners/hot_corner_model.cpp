#include "hot_corner_model.h"

#include <algorithm>

namespace hot_corners {

namespace {

bool IsValid(const RECT& area) noexcept {
    return area.left < area.right && area.top < area.bottom;
}

bool Overlaps(LONG first_start, LONG first_end, LONG second_start,
              LONG second_end) noexcept {
    return (std::max)(first_start, second_start) <
           (std::min)(first_end, second_end);
}

}  // namespace

std::array<bool, 4> ExposedCorners(std::span<const RECT> monitors,
                                   std::size_t monitor) noexcept {
    std::array<bool, 4> result{};
    if (monitor >= monitors.size() || !IsValid(monitors[monitor])) return result;

    const RECT& area = monitors[monitor];
    bool left = true, right = true, top = true, bottom = true;
    for (std::size_t index = 0; index < monitors.size(); ++index) {
        if (index == monitor || !IsValid(monitors[index])) continue;
        const RECT& other = monitors[index];
        if (other.right == area.left &&
            Overlaps(area.top, area.bottom, other.top, other.bottom))
            left = false;
        if (other.left == area.right &&
            Overlaps(area.top, area.bottom, other.top, other.bottom))
            right = false;
        if (other.bottom == area.top &&
            Overlaps(area.left, area.right, other.left, other.right))
            top = false;
        if (other.top == area.bottom &&
            Overlaps(area.left, area.right, other.left, other.right))
            bottom = false;
    }
    result[static_cast<std::size_t>(Corner::top_left)] = left && top;
    result[static_cast<std::size_t>(Corner::top_right)] = right && top;
    result[static_cast<std::size_t>(Corner::bottom_left)] = left && bottom;
    result[static_cast<std::size_t>(Corner::bottom_right)] = right && bottom;
    return result;
}

std::optional<Hit> Detect(POINT cursor, std::span<const RECT> monitors,
                          LONG tolerance) noexcept {
    tolerance = (std::max<LONG>)(1, tolerance);
    for (std::size_t index = 0; index < monitors.size(); ++index) {
        const RECT& area = monitors[index];
        if (!IsValid(area)) continue;
        if (cursor.x < area.left || cursor.x >= area.right ||
            cursor.y < area.top || cursor.y >= area.bottom) {
            continue;
        }
        const bool left = cursor.x < area.left + tolerance;
        const bool right = cursor.x >= area.right - tolerance;
        const bool top = cursor.y < area.top + tolerance;
        const bool bottom = cursor.y >= area.bottom - tolerance;
        const auto exposed = ExposedCorners(monitors, index);
        if (left && top && exposed[0]) return Hit{index, Corner::top_left};
        if (right && top && exposed[1]) return Hit{index, Corner::top_right};
        if (left && bottom && exposed[2]) return Hit{index, Corner::bottom_left};
        if (right && bottom && exposed[3]) return Hit{index, Corner::bottom_right};
    }
    return std::nullopt;
}

Action ResolveAction(const Settings& settings, Hit hit) noexcept {
    const auto corner = static_cast<std::size_t>(hit.corner);
    if (hit.monitor >= settings.monitors.size() || corner >= 4) return Action::none;
    return settings.monitors[hit.monitor].corners[corner];
}

std::optional<Hit> DwellTracker::Update(std::optional<Hit> hit,
                                        std::uint64_t now_ms,
                                        std::uint64_t dwell_ms) noexcept {
    if (hit != current_) {
        current_ = hit;
        entered_at_ = now_ms;
        fired_ = false;
    }
    if (!current_.has_value() || fired_ || now_ms - entered_at_ < dwell_ms) {
        return std::nullopt;
    }
    fired_ = true;
    return current_;
}

void DwellTracker::Reset() noexcept {
    current_.reset();
    entered_at_ = 0;
    fired_ = false;
}

}  // namespace hot_corners
