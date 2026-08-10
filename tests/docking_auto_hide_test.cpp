#include <mwtl/docking_auto_hide.h>

#include <stdexcept>

int main() {
    using namespace mwtl;
    using namespace std::chrono_literals;

    bool threw = false;
    try { DockAutoHideController invalid{{-1ms, 1ms, 1ms}}; }
    catch (const std::invalid_argument&) { threw = true; }
    if (!threw) return 1;

    DockAutoHideController controller{{250ms, 500ms, 120ms}};
    if (controller.RequestShow({}, DockAutoHideActivation::pointer) ||
        !controller.RequestShow({1}, DockAutoHideActivation::pointer) ||
        controller.GetSnapshot().state != DockAutoHideState::waiting_to_show)
        return 2;
    controller.Advance(249ms);
    if (controller.GetSnapshot().state != DockAutoHideState::waiting_to_show ||
        controller.GetSnapshot().progress != 0.0) return 3;
    controller.Advance(61ms);  // 1 ms beyond delay + 60 ms animation.
    if (controller.GetSnapshot().state != DockAutoHideState::showing ||
        controller.GetSnapshot().progress != 0.5) return 4;
    controller.Advance(60ms);
    if (controller.GetSnapshot().state != DockAutoHideState::shown ||
        controller.GetSnapshot().progress != 1.0) return 5;

    controller.SetPointerInside(true);
    controller.RequestHide();
    if (controller.GetSnapshot().state != DockAutoHideState::shown) return 6;
    controller.SetPointerInside(false);
    if (controller.GetSnapshot().state != DockAutoHideState::waiting_to_hide) return 7;
    controller.Advance(499ms);
    if (controller.GetSnapshot().state != DockAutoHideState::waiting_to_hide) return 8;
    controller.SetKeyboardFocusInside(true);
    if (controller.GetSnapshot().state != DockAutoHideState::shown) return 9;
    controller.SetKeyboardFocusInside(false);
    controller.Advance(560ms);
    if (controller.GetSnapshot().state != DockAutoHideState::hiding ||
        controller.GetSnapshot().progress != 0.5) return 10;
    controller.SetPointerInside(true);
    if (controller.GetSnapshot().state != DockAutoHideState::shown ||
        controller.GetSnapshot().progress != 1.0) return 11;
    controller.SetPointerInside(false);
    controller.Advance(620ms);
    if (controller.GetSnapshot().state != DockAutoHideState::hidden ||
        controller.GetSnapshot().panel) return 12;

    if (!controller.RequestShow({2}, DockAutoHideActivation::keyboard) ||
        controller.GetSnapshot().state != DockAutoHideState::shown) return 13;
    controller.RequestShow({3}, DockAutoHideActivation::pointer);
    if (controller.GetSnapshot().panel != DockPanelId{3} ||
        controller.GetSnapshot().state != DockAutoHideState::waiting_to_show)
        return 14;
    controller.RequestHide(true);
    if (controller.GetSnapshot().state != DockAutoHideState::hidden) return 15;

    DockAutoHideController immediate{{0ms, 0ms, 0ms}};
    if (!immediate.RequestShow({4}, DockAutoHideActivation::pointer) ||
        immediate.GetSnapshot().state != DockAutoHideState::showing) return 16;
    immediate.Advance(1ms);
    if (immediate.GetSnapshot().state != DockAutoHideState::shown) return 17;
    immediate.RequestHide();
    immediate.Advance(1ms);
    if (immediate.GetSnapshot().state != DockAutoHideState::hidden) return 18;

    DockLayoutModel layout{{10}, {100}, DockGroupRole::tool};
    if (!layout.AddPanel({{7}, L"Tool", DockPanelRole::tool}, {10})) return 19;
    DockMutation hide;
    hide.kind = DockMutationKind::auto_hide;
    hide.panel = {7};
    const auto hidden = layout.Propose(hide);
    if (!hidden || !layout.Commit(*hidden) || !layout.IsAutoHidden({7})) return 20;
    const auto pin = layout.Propose(MakePinDockMutation({7}, {10}, 0));
    if (!pin || !layout.Commit(*pin) || layout.IsAutoHidden({7}) ||
        layout.FindPanelGroup({7}) != DockGroupId{10}) return 21;
    return 0;
}
