#pragma once

#include <cstdint>
#include <span>
#include <string>

#include <mwfl/docking_workspace.h>

namespace mwfl {

struct DockMonitorWorkArea {
    std::wstring key;
    double x = 0.0;
    double y = 0.0;
    double width = 0.0;
    double height = 0.0;
    std::uint32_t dpi = 96;
    bool primary = false;
    bool IsValid() const noexcept;
};

enum class DockPlacementRecoveryStatus {
    unchanged,
    clamped,
    moved_to_available_monitor,
    no_monitors,
    invalid_argument,
};

struct DockPlacementRecoveryResult {
    DockPlacementRecoveryStatus status = DockPlacementRecoveryStatus::invalid_argument;
    DockFloatingPlacement placement{};
    explicit operator bool() const noexcept {
        return status == DockPlacementRecoveryStatus::unchanged ||
               status == DockPlacementRecoveryStatus::clamped ||
               status == DockPlacementRecoveryStatus::moved_to_available_monitor;
    }
};

// Pure, deterministic recovery in DIPs. Monitor enumeration and pixel/DIP
// conversion stay at the application/native boundary and can be injected in
// tests. The result is fully contained in one valid work area.
DockPlacementRecoveryResult RecoverDockFloatingPlacement(
    const DockFloatingPlacement& saved,
    std::span<const DockMonitorWorkArea> monitors) noexcept;

}  // namespace mwfl
