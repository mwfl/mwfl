#include "../examples/hot_corners/hot_corner_model.h"

#include <array>
#include <cstdlib>

int main() {
    using hot_corners::Corner;
    const std::array<RECT, 3> monitors{{
        {-1920, 0, 0, 1080},
        {0, 0, 2560, 1440},
        {2560, -900, 4160, 0},
    }};

    const auto left_top = hot_corners::Detect({-1920, 0}, monitors, 2);
    const auto primary_bottom = hot_corners::Detect({2559, 1439}, monitors, 2);
    const auto upper_right = hot_corners::Detect({4159, -900}, monitors, 2);
    const auto interior = hot_corners::Detect({500, 500}, monitors, 2);
    const auto seam_left = hot_corners::Detect({-1, 0}, monitors, 2);
    const auto seam_right = hot_corners::Detect({0, 0}, monitors, 2);
    const auto step_primary = hot_corners::Detect({2559, 0}, monitors, 2);
    const auto step_upper = hot_corners::Detect({2560, -1}, monitors, 2);
    const auto outside_half_open = hot_corners::Detect({4160, -900}, monitors, 2);
    if (!left_top || left_top->monitor != 0 || left_top->corner != Corner::top_left ||
        !primary_bottom || primary_bottom->monitor != 1 || primary_bottom->corner != Corner::bottom_right ||
        !upper_right || upper_right->monitor != 2 || upper_right->corner != Corner::top_right ||
        interior || seam_left || seam_right || !step_primary || step_primary->monitor != 1 ||
        step_primary->corner != Corner::top_right || !step_upper || step_upper->monitor != 2 ||
        step_upper->corner != Corner::bottom_left || outside_half_open) return EXIT_FAILURE;

    const auto left_exposed = hot_corners::ExposedCorners(monitors, 0);
    const auto primary_exposed = hot_corners::ExposedCorners(monitors, 1);
    if (!left_exposed[0] || left_exposed[1] || !primary_exposed[1] ||
        primary_exposed[0]) return EXIT_FAILURE;

    constexpr std::array<RECT, 2> separated{{{0, 0, 100, 100}, {110, 0, 210, 100}}};
    if (!hot_corners::Detect({99, 0}, separated, 1) ||
        !hot_corners::Detect({110, 0}, separated, 1)) return EXIT_FAILURE;

    hot_corners::DwellTracker dwell;
    if (dwell.Update(left_top, 100, 350) || dwell.Update(left_top, 449, 350)) return EXIT_FAILURE;
    if (!dwell.Update(left_top, 450, 350) || dwell.Update(left_top, 900, 350)) return EXIT_FAILURE;
    if (dwell.Update(std::nullopt, 901, 350) || dwell.Update(primary_bottom, 1000, 350) ||
        !dwell.Update(primary_bottom, 1350, 350)) return EXIT_FAILURE;

    hot_corners::Settings settings;
    settings.monitors.resize(3);
    settings.monitors[1].corners[3] = hot_corners::Action::none;
    settings.monitors[2].corners[1] = hot_corners::Action::show_desktop;
    if (hot_corners::ResolveAction(settings, *primary_bottom) != hot_corners::Action::none ||
        hot_corners::ResolveAction(settings, *upper_right) != hot_corners::Action::show_desktop ||
        hot_corners::ResolveAction(settings, {9, Corner::top_left}) != hot_corners::Action::none) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
