#include <mwtl/docking_floating.h>

#include <mwtl/dpi.h>

#include <algorithm>
#include <cmath>

namespace mwtl {
namespace {
constexpr wchar_t class_name[] = L"mwtl.DockFloatingWindow";

DockFloatingWindowResult Result(
    DockFloatingWindowStatus status, DWORD error = ERROR_SUCCESS) noexcept {
    return {status, error};
}
}

DockFloatingWindow::~DockFloatingWindow() noexcept { Destroy(); }

bool DockFloatingWindow::EnsureClass() noexcept {
    WNDCLASSEXW existing{sizeof(existing)};
    const HINSTANCE instance = ::GetModuleHandleW(nullptr);
    if (::GetClassInfoExW(instance, class_name, &existing)) return true;
    WNDCLASSEXW type{};
    type.cbSize = sizeof(type);
    type.hInstance = instance;
    type.lpfnWndProc = WindowProcedure;
    type.lpszClassName = class_name;
    type.hCursor = ::LoadCursorW(nullptr, IDC_ARROW);
    type.hIcon = ::LoadIconW(nullptr, IDI_APPLICATION);
    type.hbrBackground = ::GetSysColorBrush(COLOR_WINDOW);
    type.style = CS_HREDRAW | CS_VREDRAW;
    return ::RegisterClassExW(&type) != 0 || ::GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

DockFloatingWindowResult DockFloatingWindow::Create(
    HWND owner, DockFloatingWindowOptions options) noexcept {
    if (window_ || !owner || !::IsWindow(owner) || options.title.empty() ||
        !options.placement.IsValid() || options.target_dpi < 48 ||
        options.target_dpi > 960 || !std::isfinite(options.minimum_width) ||
        !std::isfinite(options.minimum_height) || options.minimum_width < 80.0 ||
        options.minimum_height < 60.0 || !EnsureClass())
        return Result(DockFloatingWindowStatus::invalid_argument, ERROR_INVALID_PARAMETER);
    DWORD process = 0;
    const DWORD thread = ::GetWindowThreadProcessId(owner, &process);
    if (thread != ::GetCurrentThreadId() || process != ::GetCurrentProcessId())
        return Result(DockFloatingWindowStatus::wrong_thread_or_process,
                      ERROR_INVALID_THREAD_ID);
    try { on_close_ = std::move(options.on_close); }
    catch (...) { return Result(DockFloatingWindowStatus::native_failure,
                                ERROR_NOT_ENOUGH_MEMORY); }
    owner_ = owner;
    thread_id_ = thread;
    process_id_ = process;
    minimum_width_ = options.minimum_width;
    minimum_height_ = options.minimum_height;
    const DpiContext dpi = DpiContext::FromDpi(options.target_dpi);
    const int x = dpi.ToPixels(Dip{static_cast<float>(options.placement.x)});
    const int y = dpi.ToPixels(Dip{static_cast<float>(options.placement.y)});
    const int width = dpi.ToPixels(Dip{static_cast<float>(options.placement.width)});
    const int height = dpi.ToPixels(Dip{static_cast<float>(options.placement.height)});
    window_ = ::CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_CONTROLPARENT,
        class_name, options.title.c_str(), WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        x, y, width, height, owner, nullptr, ::GetModuleHandleW(nullptr), this);
    if (!window_) {
        const DWORD error = ::GetLastError();
        owner_ = nullptr;
        thread_id_ = process_id_ = 0;
        return Result(DockFloatingWindowStatus::native_failure, error);
    }
    return Result(DockFloatingWindowStatus::success);
}

bool DockFloatingWindow::IsCompatible(HWND window) const noexcept {
    if (!window || !::IsWindow(window) || ::GetCurrentThreadId() != thread_id_)
        return false;
    DWORD process = 0;
    return ::GetWindowThreadProcessId(window, &process) == thread_id_ &&
           process == process_id_;
}

DockFloatingWindowResult DockFloatingWindow::AttachContent(HWND content) noexcept {
    if (!window_ || !IsCompatible(content))
        return Result(DockFloatingWindowStatus::stale_window,
                      ERROR_INVALID_WINDOW_HANDLE);
    if (content_) return Result(DockFloatingWindowStatus::already_attached,
                                ERROR_ALREADY_EXISTS);
    const HWND parent = ::GetParent(content);
    if (!IsCompatible(parent)) return Result(DockFloatingWindowStatus::stale_window,
                                             ERROR_INVALID_WINDOW_HANDLE);
    original_parent_ = parent;
    original_style_ = ::GetWindowLongPtrW(content, GWL_STYLE);
    original_extended_style_ = ::GetWindowLongPtrW(content, GWL_EXSTYLE);
    original_visible_ = ::IsWindowVisible(content) != FALSE;
    ::SetLastError(ERROR_SUCCESS);
    if (!::SetParent(content, window_) && ::GetLastError() != ERROR_SUCCESS) {
        original_parent_ = nullptr;
        return Result(DockFloatingWindowStatus::native_failure, ::GetLastError());
    }
    const LONG_PTR style = (original_style_ | WS_CHILD) & ~WS_POPUP;
    ::SetLastError(ERROR_SUCCESS);
    if (!::SetWindowLongPtrW(content, GWL_STYLE, style) &&
        ::GetLastError() != ERROR_SUCCESS) {
        ::SetParent(content, original_parent_);
        original_parent_ = nullptr;
        return Result(DockFloatingWindowStatus::native_failure, ::GetLastError());
    }
    content_ = content;
    ArrangeContent();
    ::ShowWindow(content_, SW_SHOWNA);
    return Result(DockFloatingWindowStatus::success);
}

DockFloatingWindowResult DockFloatingWindow::DetachContent() noexcept {
    if (!content_) return Result(DockFloatingWindowStatus::not_attached,
                                 ERROR_NOT_FOUND);
    if (!IsCompatible(content_) || !IsCompatible(original_parent_))
        return Result(DockFloatingWindowStatus::stale_window,
                      ERROR_INVALID_WINDOW_HANDLE);
    const HWND content = content_;
    ::SetLastError(ERROR_SUCCESS);
    if (!::SetParent(content, original_parent_) && ::GetLastError() != ERROR_SUCCESS)
        return Result(DockFloatingWindowStatus::native_failure, ::GetLastError());
    ::SetWindowLongPtrW(content, GWL_STYLE, original_style_);
    ::SetWindowLongPtrW(content, GWL_EXSTYLE, original_extended_style_);
    ::SetWindowPos(content, nullptr, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE |
        SWP_FRAMECHANGED | (original_visible_ ? SWP_SHOWWINDOW : SWP_HIDEWINDOW));
    content_ = nullptr;
    original_parent_ = nullptr;
    return Result(DockFloatingWindowStatus::success);
}

DockFloatingWindowResult DockFloatingWindow::SetPlacement(
    DockFloatingPlacement placement, UINT target_dpi) noexcept {
    if (!window_ || !placement.IsValid() || target_dpi < 48 || target_dpi > 960 ||
        ::GetCurrentThreadId() != thread_id_)
        return Result(DockFloatingWindowStatus::invalid_argument,
                      ERROR_INVALID_PARAMETER);
    const DpiContext dpi = DpiContext::FromDpi(target_dpi);
    if (!::SetWindowPos(window_, nullptr,
        dpi.ToPixels(Dip{static_cast<float>(placement.x)}),
        dpi.ToPixels(Dip{static_cast<float>(placement.y)}),
        dpi.ToPixels(Dip{static_cast<float>(placement.width)}),
        dpi.ToPixels(Dip{static_cast<float>(placement.height)}),
        SWP_NOZORDER | SWP_NOACTIVATE))
        return Result(DockFloatingWindowStatus::native_failure, ::GetLastError());
    return Result(DockFloatingWindowStatus::success);
}

void DockFloatingWindow::Show(bool activate) noexcept {
    if (window_ && ::GetCurrentThreadId() == thread_id_)
        ::ShowWindow(window_, activate ? SW_SHOW : SW_SHOWNOACTIVATE);
}

void DockFloatingWindow::Hide() noexcept {
    if (window_ && ::GetCurrentThreadId() == thread_id_) ::ShowWindow(window_, SW_HIDE);
}

void DockFloatingWindow::Destroy() noexcept {
    if (!window_) return;
    if (::GetCurrentThreadId() != thread_id_) return;
    if (content_) DetachContent();
    const HWND window = window_;
    if (::IsWindow(window)) ::DestroyWindow(window);
    if (window_ == window) window_ = nullptr;
    owner_ = nullptr;
    thread_id_ = process_id_ = 0;
}

bool DockFloatingWindow::IsVisible() const noexcept {
    return window_ && ::IsWindowVisible(window_) != FALSE;
}

void DockFloatingWindow::ArrangeContent() noexcept {
    if (!content_ || !IsCompatible(content_)) return;
    RECT client{};
    if (::GetClientRect(window_, &client))
        ::SetWindowPos(content_, nullptr, 0, 0, client.right, client.bottom,
            SWP_NOZORDER | SWP_NOACTIVATE);
}

LRESULT DockFloatingWindow::HandleClose() noexcept {
    if (dispatching_close_) return 0;
    dispatching_close_ = true;
    DockFloatingCloseAction action = DockFloatingCloseAction::hide;
    last_callback_error_ = nullptr;
    try {
        if (on_close_) action = on_close_();
    } catch (...) {
        last_callback_error_ = std::current_exception();
        action = DockFloatingCloseAction::cancel;
    }
    dispatching_close_ = false;
    if (!window_) return 0;
    if (action == DockFloatingCloseAction::hide) Hide();
    else if (action == DockFloatingCloseAction::destroy) Destroy();
    return 0;
}

LRESULT CALLBACK DockFloatingWindow::WindowProcedure(
    HWND window, UINT message, WPARAM wparam, LPARAM lparam) noexcept {
    auto* self = reinterpret_cast<DockFloatingWindow*>(
        ::GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lparam);
        self = static_cast<DockFloatingWindow*>(create->lpCreateParams);
        ::SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    if (!self) return ::DefWindowProcW(window, message, wparam, lparam);
    if (message == WM_CLOSE) return self->HandleClose();
    if (message == WM_KEYDOWN && wparam == VK_ESCAPE) return self->HandleClose();
    if (message == WM_SIZE) {
        self->ArrangeContent();
        return 0;
    }
    if (message == WM_GETMINMAXINFO) {
        auto* info = reinterpret_cast<MINMAXINFO*>(lparam);
        const DpiContext dpi = DpiContext::FromWindow(window);
        info->ptMinTrackSize.x = dpi.ToPixels(Dip{static_cast<float>(self->minimum_width_)});
        info->ptMinTrackSize.y = dpi.ToPixels(Dip{static_cast<float>(self->minimum_height_)});
        return 0;
    }
    if (message == WM_DPICHANGED) {
        const auto* suggested = reinterpret_cast<const RECT*>(lparam);
        if (suggested) ::SetWindowPos(window, nullptr, suggested->left, suggested->top,
            suggested->right - suggested->left, suggested->bottom - suggested->top,
            SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }
    if (message == WM_NCDESTROY) {
        if (self->content_) self->DetachContent();
        self->window_ = nullptr;
        self->owner_ = nullptr;
        self->thread_id_ = self->process_id_ = 0;
        ::SetWindowLongPtrW(window, GWLP_USERDATA, 0);
    }
    return ::DefWindowProcW(window, message, wparam, lparam);
}

}  // namespace mwtl
