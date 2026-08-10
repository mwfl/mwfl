#include <mwfl/docking_preview.h>

#include <mwfl/appearance.h>
#include <mwfl/dpi.h>

#include <algorithm>

namespace mwfl {
namespace {
constexpr wchar_t class_name[] = L"mwfl.DockPreviewWindow";
}

DockPreviewWindow::~DockPreviewWindow() noexcept { Destroy(); }

bool DockPreviewWindow::EnsureClass() noexcept {
    WNDCLASSEXW existing{};
    existing.cbSize = sizeof(existing);
    const HINSTANCE instance = ::GetModuleHandleW(nullptr);
    if (::GetClassInfoExW(instance, class_name, &existing)) return true;
    WNDCLASSEXW type{};
    type.cbSize = sizeof(type);
    type.hInstance = instance;
    type.lpfnWndProc = WindowProcedure;
    type.lpszClassName = class_name;
    type.hCursor = ::LoadCursorW(nullptr, IDC_ARROW);
    type.hbrBackground = nullptr;
    type.style = CS_HREDRAW | CS_VREDRAW;
    return ::RegisterClassExW(&type) != 0 || ::GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

bool DockPreviewWindow::Create(HWND owner, DockPreviewOptions options) noexcept {
    if (window_ || !owner || !::IsWindow(owner) || options.opacity == 0 ||
        options.border_pixels < 1 || options.border_pixels > 32 || !EnsureClass()) {
        ::SetLastError(ERROR_INVALID_PARAMETER);
        return false;
    }
    DWORD owner_process = 0;
    const DWORD owner_thread = ::GetWindowThreadProcessId(owner, &owner_process);
    if (owner_thread != ::GetCurrentThreadId() || owner_process != ::GetCurrentProcessId()) {
        ::SetLastError(ERROR_INVALID_THREAD_ID);
        return false;
    }
    options_ = options;
    thread_id_ = owner_thread;
    window_ = ::CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT | WS_EX_LAYERED,
        class_name, L"Docking preview", WS_POPUP, 0, 0, 0, 0, owner, nullptr,
        ::GetModuleHandleW(nullptr), this);
    if (!window_) return false;
    const BYTE opacity = IsHighContrastEnabled() ? 255 : options.opacity;
    if (!::SetLayeredWindowAttributes(window_, 0, opacity, LWA_ALPHA)) {
        Destroy();
        return false;
    }
    return true;
}

bool DockPreviewWindow::Show(DockRectDip bounds, UINT target_dpi) noexcept {
    if (!window_ || ::GetCurrentThreadId() != thread_id_ || !bounds.IsValid() ||
        target_dpi < 48 || target_dpi > 960) {
        ::SetLastError(ERROR_INVALID_PARAMETER);
        return false;
    }
    const DpiContext dpi = DpiContext::FromDpi(target_dpi);
    const int x = dpi.ToPixels(Dip{static_cast<float>(bounds.x)});
    const int y = dpi.ToPixels(Dip{static_cast<float>(bounds.y)});
    const int width = (std::max)(1, dpi.ToPixels(Dip{static_cast<float>(bounds.width)}));
    const int height = (std::max)(1, dpi.ToPixels(Dip{static_cast<float>(bounds.height)}));
    if (!::SetWindowPos(window_, HWND_TOPMOST, x, y, width, height,
            SWP_NOACTIVATE | SWP_SHOWWINDOW)) return false;
    ::InvalidateRect(window_, nullptr, TRUE);
    return true;
}

void DockPreviewWindow::Hide() noexcept {
    if (window_ && ::GetCurrentThreadId() == thread_id_)
        ::ShowWindow(window_, SW_HIDE);
}

void DockPreviewWindow::Destroy() noexcept {
    if (!window_) return;
    if (::GetCurrentThreadId() == thread_id_ && ::IsWindow(window_))
        ::DestroyWindow(window_);
    window_ = nullptr;
    thread_id_ = 0;
}

bool DockPreviewWindow::IsVisible() const noexcept {
    return window_ && ::IsWindowVisible(window_) != FALSE;
}

LRESULT CALLBACK DockPreviewWindow::WindowProcedure(
    HWND window, UINT message, WPARAM wparam, LPARAM lparam) noexcept {
    auto* self = reinterpret_cast<DockPreviewWindow*>(
        ::GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lparam);
        self = static_cast<DockPreviewWindow*>(create->lpCreateParams);
        ::SetWindowLongPtrW(window, GWLP_USERDATA,
                            reinterpret_cast<LONG_PTR>(self));
    }
    if (message == WM_NCHITTEST) return HTTRANSPARENT;
    if (message == WM_MOUSEACTIVATE) return MA_NOACTIVATE;
    if (message == WM_ERASEBKGND) return 1;
    if (message == WM_PAINT) {
        PAINTSTRUCT paint{};
        const HDC dc = ::BeginPaint(window, &paint);
        RECT client{};
        ::GetClientRect(window, &client);
        const bool high_contrast = IsHighContrastEnabled();
        const COLORREF fill = ::GetSysColor(high_contrast ? COLOR_HIGHLIGHT : COLOR_HOTLIGHT);
        const HBRUSH brush = ::CreateSolidBrush(fill);
        if (brush) {
            ::FillRect(dc, &client, brush);
            ::DeleteObject(brush);
        }
        const int border = self ? self->options_.border_pixels : 3;
        for (int i = 0; i < border; ++i) {
            ::FrameRect(dc, &client,
                ::GetSysColorBrush(high_contrast ? COLOR_WINDOWTEXT : COLOR_HIGHLIGHT));
            ::InflateRect(&client, -1, -1);
        }
        ::EndPaint(window, &paint);
        return 0;
    }
    if (message == WM_DPICHANGED) {
        const auto* suggested = reinterpret_cast<const RECT*>(lparam);
        if (suggested) ::SetWindowPos(window, HWND_TOPMOST, suggested->left, suggested->top,
            suggested->right - suggested->left, suggested->bottom - suggested->top,
            SWP_NOACTIVATE);
        return 0;
    }
    if (message == WM_NCDESTROY) {
        if (self && self->window_ == window) {
            self->window_ = nullptr;
            self->thread_id_ = 0;
        }
        ::SetWindowLongPtrW(window, GWLP_USERDATA, 0);
    }
    return ::DefWindowProcW(window, message, wparam, lparam);
}

}  // namespace mwfl
