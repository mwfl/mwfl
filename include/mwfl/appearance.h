#pragma once

#include <windows.h>

namespace mwfl {

enum class ColorMode {
    system,
    light,
    dark,
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
bool ApplyWindowAppearance(HWND window, AppearanceOptions options = {}) noexcept;

// Native controls expose their text as the default accessible name. This
// helper makes that intent explicit for controls without visible labels.
bool SetAccessibleName(HWND control, const wchar_t* name) noexcept;
bool SetDialogDefaultButton(HWND window, UINT command_id) noexcept;

}  // namespace mwfl
