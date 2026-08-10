#pragma once

#include <windows.h>

#include <mwtl/dpi.h>
#include <mwtl/appearance.h>

namespace mwtl {

struct DefaultWindowClassTraits {
    static const wchar_t* GetClassName() noexcept { return L"mwtl.MainWindow"; }
    static UINT GetClassStyle() noexcept { return CS_HREDRAW | CS_VREDRAW; }
    static HICON GetIcon() noexcept { return nullptr; }
    static HICON GetSmallIcon() noexcept { return nullptr; }
    static HCURSOR GetCursor() noexcept { return ::LoadCursorW(nullptr, IDC_ARROW); }
    static HBRUSH GetBackground() noexcept {
        return reinterpret_cast<HBRUSH>(static_cast<INT_PTR>(COLOR_WINDOW + 1));
    }
};

struct WindowOptions {
    const wchar_t* title = L"mwtl application";
    DWORD style = WS_OVERLAPPEDWINDOW;
    DWORD extended_style = 0;
    RectDip initial_bounds{{0.0_dip, 0.0_dip}, {960.0_dip, 640.0_dip}};
    bool use_default_bounds = true;
    bool center_in_work_area = true;
    bool bounds_are_client_size = true;
    bool apply_suggested_dpi_rect = true;
    // Main windows normally end the message loop. Set false for independently
    // closable auxiliary top-level windows owned by the application.
    bool quit_on_destroy = true;
    AppearanceOptions appearance{};
    HICON icon = nullptr;        // Non-owning; applied with WM_SETICON.
    HICON small_icon = nullptr;  // Non-owning; applied with WM_SETICON.
    HCURSOR cursor = nullptr;    // Non-owning; applied to the registered class.
    HBRUSH background = nullptr; // Non-owning; applied to the registered class.
};

namespace detail {

RECT ResolveWindowBounds(const WindowOptions& options) noexcept;

}  // namespace detail
}  // namespace mwtl
