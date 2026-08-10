#pragma once

#include <windows.h>

#include <mwfl/docking_drag.h>

namespace mwfl {

struct DockPreviewOptions {
    BYTE opacity = 104;
    int border_pixels = 3;
};

// Owns a non-activating, input-transparent top-level preview HWND and borrows
// its owner HWND. Bounds are virtual-screen DIPs at the explicitly supplied
// target-monitor DPI. Create/show/hide/destroy belong to one UI thread.
class DockPreviewWindow final {
public:
    DockPreviewWindow() noexcept = default;
    ~DockPreviewWindow() noexcept;
    DockPreviewWindow(const DockPreviewWindow&) = delete;
    DockPreviewWindow& operator=(const DockPreviewWindow&) = delete;
    DockPreviewWindow(DockPreviewWindow&&) = delete;
    DockPreviewWindow& operator=(DockPreviewWindow&&) = delete;

    bool Create(HWND owner, DockPreviewOptions options = {}) noexcept;
    bool Show(DockRectDip screen_bounds, UINT target_dpi) noexcept;
    void Hide() noexcept;
    void Destroy() noexcept;
    HWND GetHwnd() const noexcept { return window_; }
    bool IsVisible() const noexcept;

private:
    static LRESULT CALLBACK WindowProcedure(HWND window, UINT message,
                                            WPARAM wparam, LPARAM lparam) noexcept;
    static bool EnsureClass() noexcept;

    HWND window_ = nullptr;
    DWORD thread_id_ = 0;
    DockPreviewOptions options_{};
};

}  // namespace mwfl
