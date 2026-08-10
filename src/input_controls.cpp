#include <mwfl/input_controls.h>

namespace mwfl {
namespace {
bool Initialize(DWORD classes) noexcept { INITCOMMONCONTROLSEX value{sizeof(value), classes}; return ::InitCommonControlsEx(&value) != FALSE; }
}

bool DateTimePicker::Create(HWND parent, ControlId id, RectDip bounds, DateTimePickerOptions options) { return Initialize(ICC_DATE_CLASSES) && CreateNative(DATETIMEPICK_CLASSW, parent, id, L"", bounds, options.style, options.extended_style); }
std::optional<SYSTEMTIME> DateTimePicker::GetValue() const noexcept { SYSTEMTIME value{}; return IsWindow() && ::SendMessageW(GetHwnd(), DTM_GETSYSTEMTIME, 0, reinterpret_cast<LPARAM>(&value)) == GDT_VALID ? std::optional<SYSTEMTIME>{value} : std::nullopt; }
bool DateTimePicker::SetValue(const SYSTEMTIME& value) noexcept { return IsWindow() && ::SendMessageW(GetHwnd(), DTM_SETSYSTEMTIME, GDT_VALID, reinterpret_cast<LPARAM>(&value)) != FALSE; }

bool MonthCalendar::Create(HWND parent, ControlId id, RectDip bounds, MonthCalendarOptions options) { return Initialize(ICC_DATE_CLASSES) && CreateNative(MONTHCAL_CLASSW, parent, id, L"", bounds, options.style, options.extended_style); }
std::optional<SYSTEMTIME> MonthCalendar::GetValue() const noexcept { SYSTEMTIME value{}; return IsWindow() && ::SendMessageW(GetHwnd(), MCM_GETCURSEL, 0, reinterpret_cast<LPARAM>(&value)) != FALSE ? std::optional<SYSTEMTIME>{value} : std::nullopt; }
bool MonthCalendar::SetValue(const SYSTEMTIME& value) noexcept { return IsWindow() && ::SendMessageW(GetHwnd(), MCM_SETCURSEL, 0, reinterpret_cast<LPARAM>(&value)) != FALSE; }

bool HotKey::Create(HWND parent, ControlId id, RectDip bounds, HotKeyOptions options) { return Initialize(ICC_HOTKEY_CLASS) && CreateNative(HOTKEY_CLASSW, parent, id, L"", bounds, options.style, options.extended_style); }
HotKey& HotKey::SetValue(WORD virtual_key, WORD modifiers) noexcept { if (IsWindow()) ::SendMessageW(GetHwnd(), HKM_SETHOTKEY, MAKEWORD(virtual_key, modifiers), 0); return *this; }
DWORD HotKey::GetValue() const noexcept { return IsWindow() ? static_cast<DWORD>(::SendMessageW(GetHwnd(), HKM_GETHOTKEY, 0, 0)) : 0; }
std::optional<HotKeyValue> HotKey::GetHotKey() const noexcept {
    if (!IsWindow()) return std::nullopt;
    const DWORD value = GetValue();
    return HotKeyValue{LOBYTE(LOWORD(value)), HIBYTE(LOWORD(value))};
}

bool IpAddress::Create(HWND parent, ControlId id, RectDip bounds, IpAddressOptions options) { return Initialize(ICC_INTERNET_CLASSES) && CreateNative(WC_IPADDRESSW, parent, id, L"", bounds, options.style, options.extended_style); }
IpAddress& IpAddress::SetValue(BYTE first, BYTE second, BYTE third, BYTE fourth) noexcept { if (IsWindow()) ::SendMessageW(GetHwnd(), IPM_SETADDRESS, 0, MAKEIPADDRESS(first, second, third, fourth)); return *this; }
std::optional<DWORD> IpAddress::GetValue() const noexcept { if (!IsWindow()) return std::nullopt; DWORD value = 0; return ::SendMessageW(GetHwnd(), IPM_GETADDRESS, 0, reinterpret_cast<LPARAM>(&value)) == 4 ? std::optional<DWORD>{value} : std::nullopt; }
std::optional<IpAddressValue> IpAddress::GetAddress() const noexcept {
    const auto value = GetValue();
    if (!value) return std::nullopt;
    return IpAddressValue{{
        static_cast<BYTE>(FIRST_IPADDRESS(*value)),
        static_cast<BYTE>(SECOND_IPADDRESS(*value)),
        static_cast<BYTE>(THIRD_IPADDRESS(*value)),
        static_cast<BYTE>(FOURTH_IPADDRESS(*value))}};
}

bool UpDown::Create(HWND parent, ControlId id, RectDip bounds, UpDownOptions options) { return Initialize(ICC_UPDOWN_CLASS) && CreateNative(UPDOWN_CLASSW, parent, id, L"", bounds, options.style, options.extended_style); }
UpDown& UpDown::SetBuddy(const NativeControl& buddy) noexcept { if (IsWindow()) ::SendMessageW(GetHwnd(), UDM_SETBUDDY, reinterpret_cast<WPARAM>(buddy.GetHwnd()), 0); return *this; }
UpDown& UpDown::SetRange(int minimum, int maximum) noexcept { if (IsWindow()) ::SendMessageW(GetHwnd(), UDM_SETRANGE32, minimum, maximum); return *this; }
UpDown& UpDown::SetValue(int value) noexcept { if (IsWindow()) ::SendMessageW(GetHwnd(), UDM_SETPOS32, 0, value); return *this; }
int UpDown::GetValue() const noexcept { return IsWindow() ? static_cast<int>(::SendMessageW(GetHwnd(), UDM_GETPOS32, 0, 0)) : 0; }

bool SysLink::Create(HWND parent, ControlId id, std::wstring_view markup, RectDip bounds, SysLinkOptions options) { return Initialize(ICC_LINK_CLASS) && CreateNative(WC_LINK, parent, id, markup, bounds, options.style, options.extended_style); }

}  // namespace mwfl
