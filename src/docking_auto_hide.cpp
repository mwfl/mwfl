#include <mwtl/docking_auto_hide.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace mwtl {
namespace {

constexpr std::chrono::milliseconds maximum_duration{5000};

double Fraction(std::chrono::milliseconds elapsed,
                std::chrono::milliseconds duration) noexcept {
    if (duration.count() <= 0) return 1.0;
    return std::clamp(static_cast<double>(elapsed.count()) /
                      static_cast<double>(duration.count()), 0.0, 1.0);
}

}  // namespace

bool DockAutoHideOptions::IsValid() const noexcept {
    return pointer_show_delay.count() >= 0 && hide_delay.count() >= 0 &&
           animation_duration.count() >= 0 &&
           pointer_show_delay <= maximum_duration && hide_delay <= maximum_duration &&
           animation_duration <= maximum_duration;
}

DockAutoHideController::DockAutoHideController(DockAutoHideOptions options)
    : options_(options) {
    if (!options.IsValid()) throw std::invalid_argument("invalid auto-hide timing");
}

bool DockAutoHideController::RequestShow(
    DockPanelId panel, DockAutoHideActivation activation) noexcept {
    if (!panel) return false;
    const bool switching = state_.panel != panel;
    state_.panel = panel;
    phase_elapsed_ = {};
    if (activation == DockAutoHideActivation::keyboard ||
        activation == DockAutoHideActivation::programmatic) {
        state_.state = DockAutoHideState::shown;
        state_.progress = 1.0;
    } else if (switching || state_.state == DockAutoHideState::hidden ||
               state_.state == DockAutoHideState::hiding) {
        state_.state = options_.pointer_show_delay.count() == 0
            ? DockAutoHideState::showing : DockAutoHideState::waiting_to_show;
        state_.progress = 0.0;
    } else if (state_.state == DockAutoHideState::waiting_to_hide) {
        state_.state = DockAutoHideState::shown;
        state_.progress = 1.0;
    }
    return true;
}

void DockAutoHideController::RequestHide(bool immediate) noexcept {
    if (state_.state == DockAutoHideState::hidden) return;
    if (immediate) {
        Reset();
        return;
    }
    if (KeepsOpen()) return;
    phase_elapsed_ = {};
    state_.state = options_.hide_delay.count() == 0
        ? DockAutoHideState::hiding : DockAutoHideState::waiting_to_hide;
}

void DockAutoHideController::SetPointerInside(bool inside) noexcept {
    state_.pointer_inside = inside;
    if (inside && (state_.state == DockAutoHideState::waiting_to_hide ||
                   state_.state == DockAutoHideState::hiding)) {
        state_.state = DockAutoHideState::shown;
        state_.progress = 1.0;
        phase_elapsed_ = {};
    } else if (!inside && !KeepsOpen() && state_.state == DockAutoHideState::shown) {
        RequestHide();
    }
}

void DockAutoHideController::SetKeyboardFocusInside(bool inside) noexcept {
    state_.keyboard_focus_inside = inside;
    if (inside && (state_.state == DockAutoHideState::waiting_to_hide ||
                   state_.state == DockAutoHideState::hiding)) {
        state_.state = DockAutoHideState::shown;
        state_.progress = 1.0;
        phase_elapsed_ = {};
    } else if (!inside && !KeepsOpen() && state_.state == DockAutoHideState::shown) {
        RequestHide();
    }
}

bool DockAutoHideController::KeepsOpen() const noexcept {
    return state_.pointer_inside || state_.keyboard_focus_inside;
}

void DockAutoHideController::StartHiding() noexcept {
    state_.state = DockAutoHideState::hiding;
    phase_elapsed_ = {};
}

void DockAutoHideController::Advance(std::chrono::milliseconds elapsed) noexcept {
    if (elapsed.count() <= 0 || state_.state == DockAutoHideState::hidden ||
        state_.state == DockAutoHideState::shown) return;
    // Bound a delayed timer callback so one UI stall cannot iterate or overflow.
    elapsed = (std::min)(elapsed, maximum_duration);
    phase_elapsed_ += elapsed;
    if (state_.state == DockAutoHideState::waiting_to_show) {
        if (phase_elapsed_ < options_.pointer_show_delay) return;
        phase_elapsed_ -= options_.pointer_show_delay;
        state_.state = DockAutoHideState::showing;
    }
    if (state_.state == DockAutoHideState::showing) {
        state_.progress = Fraction(phase_elapsed_, options_.animation_duration);
        if (state_.progress >= 1.0) {
            state_.state = DockAutoHideState::shown;
            phase_elapsed_ = {};
        }
        return;
    }
    if (state_.state == DockAutoHideState::waiting_to_hide) {
        if (KeepsOpen()) {
            state_.state = DockAutoHideState::shown;
            state_.progress = 1.0;
            phase_elapsed_ = {};
            return;
        }
        if (phase_elapsed_ < options_.hide_delay) return;
        phase_elapsed_ -= options_.hide_delay;
        state_.state = DockAutoHideState::hiding;
    }
    if (state_.state == DockAutoHideState::hiding) {
        state_.progress = 1.0 - Fraction(phase_elapsed_, options_.animation_duration);
        if (state_.progress <= 0.0) Reset();
    }
}

void DockAutoHideController::Reset() noexcept {
    state_ = {};
    phase_elapsed_ = {};
}

DockMutation MakePinDockMutation(
    DockPanelId panel, DockGroupId destination,
    std::optional<std::size_t> index) noexcept {
    DockMutation mutation;
    mutation.kind = DockMutationKind::move_to_group;
    mutation.panel = panel;
    mutation.target_group = destination;
    mutation.target_index = index;
    return mutation;
}

}  // namespace mwtl
