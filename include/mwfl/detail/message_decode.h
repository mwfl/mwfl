#pragma once

#include <windows.h>

#include <mwfl/events.h>

namespace mwfl::detail {

constexpr KeyEvent DecodeKeyEvent(WPARAM wparam, LPARAM lparam) noexcept {
    return {
        .virtual_key = static_cast<UINT>(wparam),
        .repeat_count = static_cast<std::uint16_t>(lparam & 0xFFFF),
        .scan_code = static_cast<std::uint8_t>((lparam >> 16) & 0xFF),
        .extended = (lparam & (1LL << 24)) != 0,
        .was_down = (lparam & (1LL << 30)) != 0,
    };
}

constexpr MouseEvent DecodeMouseEvent(WPARAM wparam, LPARAM lparam) noexcept {
    return {
        .position = POINT{static_cast<short>(LOWORD(lparam)),
                          static_cast<short>(HIWORD(lparam))},
        .key_state = wparam,
    };
}

inline ScrollEvent DecodeScrollEvent(
    UINT message, WPARAM wparam, LPARAM lparam) noexcept {
    return {
        .control = reinterpret_cast<HWND>(lparam),
        .request = LOWORD(wparam),
        .position = HIWORD(wparam),
        .horizontal = message == WM_HSCROLL,
    };
}

constexpr ResizeEvent DecodeResizeEvent(WPARAM wparam, LPARAM lparam) noexcept {
    return {
        .client_size = SIZE{static_cast<LONG>(LOWORD(lparam)),
                            static_cast<LONG>(HIWORD(lparam))},
        .state = DecodeWindowSizeState(wparam),
    };
}

inline CommandEvent DecodeCommandEvent(WPARAM wparam, LPARAM lparam) noexcept {
    return {
        .id = ControlId{LOWORD(wparam)},
        .notification = HIWORD(wparam),
        .control = reinterpret_cast<HWND>(lparam),
    };
}

constexpr WindowMessage DecodeWindowMessage(
    UINT message, WPARAM wparam, LPARAM lparam) noexcept {
    return {message, wparam, lparam};
}

}  // namespace mwfl::detail
