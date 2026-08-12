#pragma once

#include <windows.h>

namespace mwfl {

enum class ColorMode {
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
AppearanceState ResolveAppearance(AppearanceOptions options = {}) noexcept;
AppearanceState GetWindowAppearance(HWND window) noexcept;
bool ApplyWindowAppearance(HWND window, AppearanceOptions options = {}) noexcept;
// Applies Explorer-compatible visual styles to an existing control tree and
// redraws the attached menu. HWNDs remain borrowed and on their UI thread.
bool ApplyNativeControlAppearance(HWND window, const AppearanceState& state) noexcept;

// Native controls expose their text as the default accessible name. This
// helper makes that intent explicit for controls without visible labels.
bool SetAccessibleName(HWND control, const wchar_t* name) noexcept;
bool SetDialogDefaultButton(HWND window, UINT command_id) noexcept;

}  // namespace mwfl
