#include <mwfl/docking_monitor.h>

#include <array>
#include <cmath>

int main() {
    using namespace mwfl;
    const std::array monitors{
        DockMonitorWorkArea{L"left", -1920, 0, 1920, 1040, 96, false},
        DockMonitorWorkArea{L"primary", 0, 0, 2560, 1400, 144, true},
    };
    const DockFloatingPlacement normal{100, 80, 700, 500, L"primary"};
    const auto unchanged = RecoverDockFloatingPlacement(normal, monitors);
    if (!unchanged || unchanged.status != DockPlacementRecoveryStatus::unchanged ||
        unchanged.placement.x != normal.x) return 1;

    const DockFloatingPlacement outside{2400, 1350, 900, 700, L"primary"};
    const auto clamped = RecoverDockFloatingPlacement(outside, monitors);
    if (!clamped || clamped.status != DockPlacementRecoveryStatus::clamped ||
        clamped.placement.x != 1660 || clamped.placement.y != 700) return 2;

    const DockFloatingPlacement removed{-1800, 100, 500, 400, L"removed"};
    const auto nearest = RecoverDockFloatingPlacement(removed, monitors);
    if (!nearest || nearest.status !=
            DockPlacementRecoveryStatus::moved_to_available_monitor ||
        nearest.placement.monitor_key != L"left" || nearest.placement.x != -1800)
        return 3;

    const DockFloatingPlacement huge{-5000, -5000, 9000, 9000, L"missing"};
    const auto fitted = RecoverDockFloatingPlacement(huge, monitors);
    if (!fitted || fitted.placement.width != 1920 || fitted.placement.height != 1040 ||
        fitted.placement.x != -1920 || fitted.placement.y != 0) return 4;

    if (RecoverDockFloatingPlacement(normal, {}).status !=
        DockPlacementRecoveryStatus::no_monitors) return 5;
    auto invalid = monitors;
    invalid[0].dpi = 0;
    if (RecoverDockFloatingPlacement(normal, invalid).status !=
        DockPlacementRecoveryStatus::invalid_argument) return 6;
    auto bad_saved = normal;
    bad_saved.width = std::nan("");
    if (RecoverDockFloatingPlacement(bad_saved, monitors).status !=
        DockPlacementRecoveryStatus::invalid_argument) return 7;

    const std::array tie{
        DockMonitorWorkArea{L"secondary", -1000, 0, 1000, 800, 96, false},
        DockMonitorWorkArea{L"primary", 1000, 0, 1000, 800, 96, true},
    };
    const DockFloatingPlacement centered{400, 300, 200, 200, L"gone"};
    const auto tie_result = RecoverDockFloatingPlacement(centered, tie);
    if (!tie_result || tie_result.placement.monitor_key != L"primary") return 8;
    return 0;
}
