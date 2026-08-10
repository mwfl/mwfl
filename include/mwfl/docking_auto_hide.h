#pragma once

#include <chrono>
#include <optional>

#include <mwfl/docking_workspace.h>

namespace mwfl {

enum class DockAutoHideState {
    hidden,
    waiting_to_show,
    showing,
    shown,
    waiting_to_hide,
    hiding,
};

enum class DockAutoHideActivation { pointer, keyboard, programmatic };

struct DockAutoHideOptions {
    std::chrono::milliseconds pointer_show_delay{250};
    std::chrono::milliseconds hide_delay{500};
    std::chrono::milliseconds animation_duration{120};
    bool IsValid() const noexcept;
};

struct DockAutoHideSnapshot {
    DockAutoHideState state = DockAutoHideState::hidden;
    std::optional<DockPanelId> panel;
    double progress = 0.0;
    bool pointer_inside = false;
    bool keyboard_focus_inside = false;
};

// Pure deterministic controller. The application advances it from a bounded UI
// timer and projects progress to its borrowed panel HWND. Keyboard activation
// is immediate and never requires hover. The model owns no timers or HWNDs.
class DockAutoHideController final {
public:
    explicit DockAutoHideController(DockAutoHideOptions options = {});

    bool RequestShow(DockPanelId panel, DockAutoHideActivation activation) noexcept;
    void RequestHide(bool immediate = false) noexcept;
    void SetPointerInside(bool inside) noexcept;
    void SetKeyboardFocusInside(bool inside) noexcept;
    void Advance(std::chrono::milliseconds elapsed) noexcept;
    void Reset() noexcept;

    const DockAutoHideSnapshot& GetSnapshot() const noexcept { return state_; }
    DockAutoHideOptions GetOptions() const noexcept { return options_; }

private:
    bool KeepsOpen() const noexcept;
    void StartHiding() noexcept;

    DockAutoHideOptions options_{};
    DockAutoHideSnapshot state_{};
    std::chrono::milliseconds phase_elapsed_{};
};

DockMutation MakePinDockMutation(
    DockPanelId panel, DockGroupId destination,
    std::optional<std::size_t> index = std::nullopt) noexcept;

}  // namespace mwfl
