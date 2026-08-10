#pragma once

#include <windows.h>
#include <commctrl.h>

#include <concepts>
#include <optional>
#include <array>
#include <string_view>

#include <mwfl/concepts.h>
#include <mwfl/controls.h>

namespace mwfl {

struct HotKeyValue {
    WORD virtual_key = 0;
    WORD modifiers = 0;
};

struct IpAddressValue {
    std::array<BYTE, 4> octets{};
};

struct DateTimePickerOptions { DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | DTS_SHORTDATECENTURYFORMAT; DWORD extended_style = 0; };
class DateTimePicker final : public NativeControl {
public:
    bool Create(HWND parent, ControlId id, RectDip bounds, DateTimePickerOptions options = {});
    template <WindowLike Parent>
    bool Create(const Parent& parent, ControlId id, RectDip bounds, DateTimePickerOptions options = {}) { return Create(parent.GetHwnd(), id, bounds, options); }
    std::optional<SYSTEMTIME> GetValue() const noexcept;
    bool SetValue(const SYSTEMTIME& value) noexcept;
};

struct MonthCalendarOptions { DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP; DWORD extended_style = 0; };
class MonthCalendar final : public NativeControl {
public:
    bool Create(HWND parent, ControlId id, RectDip bounds, MonthCalendarOptions options = {});
    template <WindowLike Parent>
    bool Create(const Parent& parent, ControlId id, RectDip bounds, MonthCalendarOptions options = {}) { return Create(parent.GetHwnd(), id, bounds, options); }
    std::optional<SYSTEMTIME> GetValue() const noexcept;
    bool SetValue(const SYSTEMTIME& value) noexcept;
};

struct HotKeyOptions { DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP; DWORD extended_style = WS_EX_CLIENTEDGE; };
class HotKey final : public NativeControl {
public:
    bool Create(HWND parent, ControlId id, RectDip bounds, HotKeyOptions options = {});
    template <WindowLike Parent>
    bool Create(const Parent& parent, ControlId id, RectDip bounds, HotKeyOptions options = {}) { return Create(parent.GetHwnd(), id, bounds, options); }
    HotKey& SetValue(WORD virtual_key, WORD modifiers) noexcept;
    HotKey& SetValue(HotKeyValue value) noexcept {
        return SetValue(value.virtual_key, value.modifiers);
    }
    std::optional<HotKeyValue> GetHotKey() const noexcept;
    DWORD GetValue() const noexcept;
};

struct IpAddressOptions { DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP; DWORD extended_style = WS_EX_CLIENTEDGE; };
class IpAddress final : public NativeControl {
public:
    bool Create(HWND parent, ControlId id, RectDip bounds, IpAddressOptions options = {});
    template <WindowLike Parent>
    bool Create(const Parent& parent, ControlId id, RectDip bounds, IpAddressOptions options = {}) { return Create(parent.GetHwnd(), id, bounds, options); }
    IpAddress& SetValue(BYTE first, BYTE second, BYTE third, BYTE fourth) noexcept;
    IpAddress& SetValue(IpAddressValue value) noexcept {
        return SetValue(
            value.octets[0], value.octets[1],
            value.octets[2], value.octets[3]);
    }
    std::optional<IpAddressValue> GetAddress() const noexcept;
    std::optional<DWORD> GetValue() const noexcept;
};

struct UpDownOptions { DWORD style = WS_CHILD | WS_VISIBLE | UDS_ALIGNRIGHT | UDS_ARROWKEYS | UDS_SETBUDDYINT; DWORD extended_style = 0; };
class UpDown final : public NativeControl {
public:
    bool Create(HWND parent, ControlId id, RectDip bounds, UpDownOptions options = {});
    template <WindowLike Parent>
    bool Create(const Parent& parent, ControlId id, RectDip bounds, UpDownOptions options = {}) { return Create(parent.GetHwnd(), id, bounds, options); }
    UpDown& SetBuddy(const NativeControl& buddy) noexcept;
    UpDown& SetRange(int minimum, int maximum) noexcept;
    UpDown& SetValue(int value) noexcept;
    int GetValue() const noexcept;
};

struct SysLinkOptions { DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP; DWORD extended_style = 0; };
class SysLink final : public NativeControl {
public:
    bool Create(HWND parent, ControlId id, std::wstring_view markup, RectDip bounds, SysLinkOptions options = {});
    template <WindowLike Parent>
    bool Create(const Parent& parent, ControlId id, std::wstring_view markup, RectDip bounds, SysLinkOptions options = {}) { return Create(parent.GetHwnd(), id, markup, bounds, options); }
};

}  // namespace mwfl
