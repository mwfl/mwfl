#pragma once

#include <windows.h>

namespace mwfl {

enum class ColorMode {
    // Follow Windows and re-resolve when WindowBase receives a theme change.
    system,
    light,
    dark,
};

struct AppearancePalette {
    COLORREF window_background = RGB(255, 255, 255);
    COLORREF control_background = RGB(255, 255, 255);
    COLORREF text = RGB(0, 0, 0);
    COLORREF disabled_text = RGB(109, 109, 109);
    COLORREF accent = RGB(0, 120, 215);
};

struct AppearanceState {
    // requested_mode is policy; effective_mode is the palette safe to paint.
    // High contrast always takes precedence over an application preference.
    ColorMode requested_mode = ColorMode::system;
    ColorMode effective_mode = ColorMode::light;
    bool high_contrast = false;
    AppearancePalette palette{};

    constexpr bool IsDark() const noexcept {
        return !high_contrast && effective_mode == ColorMode::dark;
    }
};

enum class Backdrop {
    none,
    mica,
    acrylic,
    tabbed,
};

struct AppearanceOptions {
    ColorMode color_mode = ColorMode::system;
    Backdrop backdrop = Backdrop::none;
    bool rounded_corners = true;
};

bool IsHighContrastEnabled() noexcept;
bool IsSystemDarkModePreferred() noexcept;
// Resolves policy without mutating a window, useful to custom-drawn controls.
AppearanceState ResolveAppearance(AppearanceOptions options = {}) noexcept;
// Inherits mwfl state through parents, or falls back to the current system state.
AppearanceState GetWindowAppearance(HWND window) noexcept;
// Requests the supported native appearance for a borrowed HWND. A true result
// means that the request was accepted; individual DWM attributes remain
// best-effort and may be unavailable or rejected by the current Windows build.
// WindowBase applications should use WindowBase::SetAppearance so the policy is
// retained and reapplied when Windows broadcasts a theme change.
bool ApplyWindowAppearanceBestEffort(
    HWND window, AppearanceOptions options = {}) noexcept;
// Applies Explorer-compatible visual styles to an existing control tree and
// redraws the attached menu. HWNDs remain borrowed and on their UI thread.
bool ApplyNativeControlAppearance(HWND window, const AppearanceState& state) noexcept;

// Native controls expose their text as the default accessible name. This
// helper makes that intent explicit for controls without visible labels.
bool SetAccessibleName(HWND control, const wchar_t* name) noexcept;
bool SetDialogDefaultButton(HWND window, UINT command_id) noexcept;

}  // namespace mwfl
