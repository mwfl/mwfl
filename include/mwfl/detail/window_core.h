#pragma once
#include <windows.h>

namespace mwfl::detail {

struct WindowMarker {};

class WindowCore {
protected:
    HWND CreateNativeWindow(const WNDCLASSEXW& descriptor, HWND parent,
                            const RECT& bounds, const wchar_t* title,
                            DWORD style, DWORD extended_style,
                            HMENU menu, void* object) noexcept;
    HWND window_ = nullptr;

private:
    virtual LRESULT DispatchNativeWindowMessage(
        HWND window, UINT message, WPARAM wparam, LPARAM lparam) noexcept = 0;
    static LRESULT CALLBACK InitialWindowProc(
        HWND window, UINT message, WPARAM wparam, LPARAM lparam) noexcept;
};

}  // namespace mwfl::detail
