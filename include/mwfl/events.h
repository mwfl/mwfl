#pragma once

#include <windows.h>

#include <compare>
#include <concepts>
#include <cstdint>

#include <mwfl/concepts.h>

namespace mwfl {

struct EventResult {
    bool handled = false;
    LRESULT result = 0;

    static constexpr EventResult Handled(
        LRESULT value = 0) noexcept {
        return {true, value};
    }

    static constexpr EventResult Propagate() noexcept {
        return {};
    }
};

struct ControlId {
    int value = 0;

    constexpr auto operator<=>(const ControlId&) const noexcept = default;
};

struct TimerId {
    UINT_PTR value = 0;

    constexpr auto operator<=>(const TimerId&) const noexcept = default;
    constexpr explicit operator bool() const noexcept {
        return value != 0;
    }
};

enum class WindowSizeState : std::uint8_t {
    restored,
    minimized,
    maximized,
    maximized_hidden,
    maximized_shown,
    unknown,
};

struct KeyEvent {
    UINT virtual_key = 0;
    std::uint16_t repeat_count = 0;
    std::uint8_t scan_code = 0;
    bool extended = false;
    bool was_down = false;
};

struct MouseEvent {
    POINT position{};
    WPARAM key_state = 0;
};

struct ResizeEvent {
    SIZE client_size{};
    WindowSizeState state = WindowSizeState::unknown;
};

struct ScrollEvent {
    HWND control = nullptr;
    int request = 0;
    int position = 0;
    bool horizontal = true;

    template <WindowLike Control>
    bool IsFrom(const Control& expected) const noexcept {
        return control != nullptr && control == expected.GetHwnd();
    }
};

struct DpiChangedEvent {
    UINT dpi_x = 96;
    UINT dpi_y = 96;
    RECT suggested_bounds{};
};

struct CommandEvent {
    ControlId id{};
    UINT notification = 0;
    HWND control = nullptr;  // Non-owning.

    constexpr bool Is(
        ControlId expected_id,
        UINT expected_notification) const noexcept {
        return id == expected_id && notification == expected_notification;
    }

    template <WindowLike Control>
    bool IsFrom(const Control& expected) const noexcept {
        return control != nullptr && control == expected.GetHwnd();
    }

    template <WindowLike Control>
    bool Is(
        const Control& expected,
        UINT expected_notification) const noexcept {
        return notification == expected_notification && IsFrom(expected);
    }

    template <WindowLike Control>
    bool IsClicked(const Control& expected) const noexcept {
        return notification == BN_CLICKED && IsFrom(expected);
    }
};

struct NotifyEvent {
    const NMHDR& header;

    constexpr ControlId GetId() const noexcept {
        return ControlId{static_cast<int>(header.idFrom)};
    }
    constexpr UINT GetCode() const noexcept { return header.code; }
    constexpr HWND GetControl() const noexcept { return header.hwndFrom; }

    template <WindowLike Control>
    bool IsFrom(const Control& expected) const noexcept {
        return header.hwndFrom != nullptr &&
               header.hwndFrom == expected.GetHwnd();
    }

    template <WindowLike Control>
    bool Is(
        const Control& expected,
        UINT expected_code) const noexcept {
        return header.code == expected_code && IsFrom(expected);
    }

    template <typename Notification>
    const Notification* As(UINT expected_code) const noexcept {
        return header.code == expected_code
            ? reinterpret_cast<const Notification*>(&header)
            : nullptr;
    }
};

struct MinMaxInfoEvent {
    MINMAXINFO& info;
};

class PaintEvent final {
public:
    explicit PaintEvent(HWND window) noexcept
        : window_(window), dc_(::BeginPaint(window, &paint_)) {}
    ~PaintEvent() noexcept {
        if (dc_ != nullptr) {
            ::EndPaint(window_, &paint_);
        }
    }

    PaintEvent(const PaintEvent&) = delete;
    PaintEvent& operator=(const PaintEvent&) = delete;
    PaintEvent(PaintEvent&&) = delete;
    PaintEvent& operator=(PaintEvent&&) = delete;

    HDC GetDC() const noexcept { return dc_; }
    const RECT& GetPaintBounds() const noexcept {
        return paint_.rcPaint;
    }
    explicit operator bool() const noexcept {
        return dc_ != nullptr;
    }

private:
    HWND window_ = nullptr;
    PAINTSTRUCT paint_{};
    HDC dc_ = nullptr;
};

struct WindowMessage {
    UINT id = 0;
    WPARAM wparam = 0;
    LPARAM lparam = 0;
};

namespace detail {

constexpr WindowSizeState DecodeWindowSizeState(
    WPARAM value) noexcept {
    switch (value) {
    case SIZE_RESTORED: return WindowSizeState::restored;
    case SIZE_MINIMIZED: return WindowSizeState::minimized;
    case SIZE_MAXIMIZED: return WindowSizeState::maximized;
    case SIZE_MAXHIDE: return WindowSizeState::maximized_hidden;
    case SIZE_MAXSHOW: return WindowSizeState::maximized_shown;
    default: return WindowSizeState::unknown;
    }
}

}  // namespace detail
}  // namespace mwfl
