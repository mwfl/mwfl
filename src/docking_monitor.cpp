#include <mwtl/docking_monitor.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace mwtl {
namespace {

bool SamePlacement(const DockFloatingPlacement& left,
                   const DockFloatingPlacement& right) noexcept {
    return left.x == right.x && left.y == right.y &&
           left.width == right.width && left.height == right.height &&
           left.monitor_key == right.monitor_key;
}

double DistanceSquared(double x, double y, const DockMonitorWorkArea& monitor) noexcept {
    const double center_x = monitor.x + monitor.width / 2.0;
    const double center_y = monitor.y + monitor.height / 2.0;
    const double dx = x - center_x;
    const double dy = y - center_y;
    return dx * dx + dy * dy;
}

}  // namespace

bool DockMonitorWorkArea::IsValid() const noexcept {
    return !key.empty() && key.size() <= 256 && std::isfinite(x) &&
           std::isfinite(y) && std::isfinite(width) && std::isfinite(height) &&
           width >= 80.0 && height >= 60.0 && width <= 100000.0 &&
           height <= 100000.0 && dpi >= 48 && dpi <= 960;
}

DockPlacementRecoveryResult RecoverDockFloatingPlacement(
    const DockFloatingPlacement& saved,
    std::span<const DockMonitorWorkArea> monitors) noexcept {
    if (!saved.IsValid()) return {DockPlacementRecoveryStatus::invalid_argument};
    if (monitors.empty()) return {DockPlacementRecoveryStatus::no_monitors};
    for (const auto& monitor : monitors) {
        if (!monitor.IsValid())
            return {DockPlacementRecoveryStatus::invalid_argument};
    }

    const DockMonitorWorkArea* target = nullptr;
    for (const auto& monitor : monitors) {
        if (monitor.key == saved.monitor_key) {
            target = &monitor;
            break;
        }
    }
    bool moved = target == nullptr;
    if (!target) {
        const double saved_x = saved.x + saved.width / 2.0;
        const double saved_y = saved.y + saved.height / 2.0;
        double nearest = (std::numeric_limits<double>::max)();
        for (const auto& monitor : monitors) {
            const double distance = DistanceSquared(saved_x, saved_y, monitor);
            if (distance < nearest ||
                (target && distance == nearest && monitor.primary && !target->primary)) {
                nearest = distance;
                target = &monitor;
            }
        }
    }

    DockFloatingPlacement recovered = saved;
    recovered.monitor_key = target->key;
    recovered.width = std::clamp(recovered.width, 80.0, target->width);
    recovered.height = std::clamp(recovered.height, 60.0, target->height);
    recovered.x = std::clamp(recovered.x, target->x,
                             target->x + target->width - recovered.width);
    recovered.y = std::clamp(recovered.y, target->y,
                             target->y + target->height - recovered.height);
    if (moved) return {DockPlacementRecoveryStatus::moved_to_available_monitor,
                       std::move(recovered)};
    return {SamePlacement(saved, recovered) ? DockPlacementRecoveryStatus::unchanged
                                            : DockPlacementRecoveryStatus::clamped,
            std::move(recovered)};
}

}  // namespace mwtl
