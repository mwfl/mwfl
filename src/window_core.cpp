#include <mwfl/detail/window_core.h>

namespace mwfl::detail {

HWND WindowCore::CreateNativeWindow(const WNDCLASSEXW& descriptor, HWND parent,
                                    const RECT& bounds, const wchar_t* title,
                                    DWORD style, DWORD extended_style,
                                    HMENU menu, void* object) noexcept {
    if (window_ != nullptr) {
        ::SetLastError(ERROR_INVALID_STATE);
        return nullptr;
    }
    WNDCLASSEXW existing{sizeof(existing)};
    if (::GetClassInfoExW(descriptor.hInstance, descriptor.lpszClassName, &existing) == FALSE) {
        WNDCLASSEXW registered = descriptor;
        registered.lpfnWndProc = &InitialWindowProc;
        if (::RegisterClassExW(&registered) == 0) return nullptr;
    } else if (existing.lpfnWndProc != &InitialWindowProc) {
        ::SetLastError(ERROR_CLASS_ALREADY_EXISTS);
        return nullptr;
    }
    const int width = bounds.left == CW_USEDEFAULT ? CW_USEDEFAULT : bounds.right - bounds.left;
    const int height = bounds.top == CW_USEDEFAULT ? CW_USEDEFAULT : bounds.bottom - bounds.top;
    return ::CreateWindowExW(extended_style, descriptor.lpszClassName, title, style,
                             bounds.left, bounds.top, width, height, parent, menu,
                             descriptor.hInstance, object);
}

LRESULT CALLBACK WindowCore::InitialWindowProc(HWND window, UINT message,
                                                WPARAM wparam, LPARAM lparam) noexcept {
    WindowCore* self = nullptr;
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lparam);
        self = static_cast<WindowCore*>(create->lpCreateParams);
        self->window_ = window;
        ::SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<WindowCore*>(::GetWindowLongPtrW(window, GWLP_USERDATA));
    }
    if (self == nullptr) return ::DefWindowProcW(window, message, wparam, lparam);
    const LRESULT result = self->DispatchNativeWindowMessage(window, message, wparam, lparam);
    if (message == WM_NCDESTROY) {
        ::SetWindowLongPtrW(window, GWLP_USERDATA, 0);
        self->window_ = nullptr;
    }
    return result;
}

}  // namespace mwfl::detail
