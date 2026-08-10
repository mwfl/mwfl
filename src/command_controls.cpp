#include <mwtl/command_controls.h>

#include <string>

namespace mwtl {
namespace { bool Initialize(DWORD classes) noexcept { INITCOMMONCONTROLSEX value{sizeof(value), classes}; return ::InitCommonControlsEx(&value) != FALSE; } }

bool Toolbar::Create(HWND parent, ControlId id, RectDip bounds, ToolbarOptions options) {
    if (!Initialize(ICC_BAR_CLASSES) || !CreateNative(TOOLBARCLASSNAMEW, parent, id, L"", bounds, options.style, options.extended_style)) return false;
    ::SendMessageW(GetHwnd(), TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);
    return true;
}
bool Toolbar::AddTextButton(ControlId command, std::wstring_view text) {
    if (!IsWindow()) return false;
    std::wstring value{text};
    const LRESULT string_index = ::SendMessageW(GetHwnd(), TB_ADDSTRINGW, 0, reinterpret_cast<LPARAM>(value.c_str()));
    if (string_index < 0) return false;
    TBBUTTON button{}; button.iBitmap = I_IMAGENONE; button.idCommand = command.value; button.fsState = TBSTATE_ENABLED; button.fsStyle = BTNS_BUTTON | BTNS_AUTOSIZE | BTNS_SHOWTEXT; button.iString = string_index;
    return ::SendMessageW(GetHwnd(), TB_ADDBUTTONSW, 1, reinterpret_cast<LPARAM>(&button)) != FALSE;
}
bool Toolbar::AddCommand(const Command& command) {
    if (!IsWindow()) return false;
    if (command.GetImageIndex() && *command.GetImageIndex() < 0) return false;
    const std::wstring text{command.GetText()};
    const LRESULT string_index = ::SendMessageW(
        GetHwnd(), TB_ADDSTRINGW, 0, reinterpret_cast<LPARAM>(text.c_str()));
    if (string_index < 0) return false;
    TBBUTTON button{};
    button.iBitmap = command.GetImageIndex().value_or(I_IMAGENONE);
    button.idCommand = command.GetId().value;
    button.fsState = TBSTATE_ENABLED;
    button.fsStyle = BTNS_BUTTON | BTNS_AUTOSIZE | BTNS_SHOWTEXT;
    button.iString = string_index;
    return ::SendMessageW(GetHwnd(), TB_ADDBUTTONSW, 1,
                          reinterpret_cast<LPARAM>(&button)) != FALSE &&
        UpdateCommand(command);
}
bool Toolbar::UpdateCommand(const Command& command) noexcept {
    if (!IsWindow()) return false;
    if (command.GetImageIndex() && *command.GetImageIndex() < 0) return false;
    const WPARAM id = static_cast<WPARAM>(command.GetId().value);
    const LRESULT enabled = ::SendMessageW(
        GetHwnd(), TB_ENABLEBUTTON, id, MAKELPARAM(command.IsEnabled(), 0));
    const LRESULT checked = ::SendMessageW(
        GetHwnd(), TB_CHECKBUTTON, id, MAKELPARAM(command.IsChecked(), 0));
    const LRESULT visible = ::SendMessageW(
        GetHwnd(), TB_HIDEBUTTON, id, MAKELPARAM(!command.IsVisible(), 0));
    const std::wstring text{command.GetText()};
    TBBUTTONINFOW info{};
    info.cbSize = sizeof(info);
    info.dwMask = TBIF_TEXT | TBIF_IMAGE;
    info.pszText = const_cast<wchar_t*>(text.c_str());
    info.iImage = command.GetImageIndex().value_or(I_IMAGENONE);
    const LRESULT named = ::SendMessageW(
        GetHwnd(), TB_SETBUTTONINFOW, id, reinterpret_cast<LPARAM>(&info));
    return enabled != FALSE && checked != FALSE && visible != FALSE &&
        named != FALSE;
}
bool Toolbar::SetImageList(const ImageList& images) noexcept {
    if (!IsWindow() || images.GetHandle() == nullptr) return false;
    ::SendMessageW(GetHwnd(), TB_SETIMAGELIST, 0,
                   reinterpret_cast<LPARAM>(images.GetHandle()));
    return true;
}
Toolbar& Toolbar::AutoSize() noexcept { if (IsWindow()) ::SendMessageW(GetHwnd(), TB_AUTOSIZE, 0, 0); return *this; }

bool StatusBar::Create(HWND parent, ControlId id, RectDip bounds, StatusBarOptions options) { return Initialize(ICC_BAR_CLASSES) && CreateNative(STATUSCLASSNAMEW, parent, id, L"", bounds, options.style, options.extended_style); }
bool StatusBar::SetParts(std::span<const int> right_edges) noexcept { return IsWindow() && ::SendMessageW(GetHwnd(), SB_SETPARTS, right_edges.size(), reinterpret_cast<LPARAM>(right_edges.data())) != FALSE; }
bool StatusBar::SetPartText(int part, std::wstring_view text) { if (!IsWindow()) return false; std::wstring value{text}; return ::SendMessageW(GetHwnd(), SB_SETTEXTW, part, reinterpret_cast<LPARAM>(value.c_str())) != FALSE; }

bool Rebar::Create(HWND parent, ControlId id, RectDip bounds, RebarOptions options) { return Initialize(ICC_COOL_CLASSES) && CreateNative(REBARCLASSNAMEW, parent, id, L"", bounds, options.style, options.extended_style); }
bool Rebar::AddBand(const NativeControl& child, std::wstring_view text, int minimum_width) { if (!IsWindow() || !child.IsWindow()) return false; RECT child_bounds{}; if (::GetWindowRect(child.GetHwnd(), &child_bounds) == FALSE) return false; std::wstring value{text}; REBARBANDINFOW band{}; band.cbSize = sizeof(band); band.fMask = RBBIM_CHILD | RBBIM_CHILDSIZE | RBBIM_SIZE; if (!value.empty()) { band.fMask |= RBBIM_TEXT; band.lpText = value.data(); } band.hwndChild = child.GetHwnd(); band.cxMinChild = minimum_width; band.cyMinChild = child_bounds.bottom - child_bounds.top; band.cx = minimum_width; return ::SendMessageW(GetHwnd(), RB_INSERTBANDW, static_cast<WPARAM>(-1), reinterpret_cast<LPARAM>(&band)) != FALSE; }

bool Pager::Create(HWND parent, ControlId id, RectDip bounds, PagerOptions options) { return Initialize(ICC_PAGESCROLLER_CLASS) && CreateNative(WC_PAGESCROLLERW, parent, id, L"", bounds, options.style, options.extended_style); }
Pager& Pager::SetChild(const NativeControl& child) noexcept { if (IsWindow()) ::SendMessageW(GetHwnd(), PGM_SETCHILD, 0, reinterpret_cast<LPARAM>(child.GetHwnd())); return *this; }
Pager& Pager::SetButtonSize(int pixels) noexcept { if (IsWindow()) ::SendMessageW(GetHwnd(), PGM_SETBUTTONSIZE, 0, pixels); return *this; }

bool Animation::Create(HWND parent, ControlId id, RectDip bounds, AnimationOptions options) { return Initialize(ICC_ANIMATE_CLASS) && CreateNative(ANIMATE_CLASSW, parent, id, L"", bounds, options.style, options.extended_style); }
bool Animation::Open(HINSTANCE module, UINT resource_id) noexcept { return IsWindow() && Animate_OpenEx(GetHwnd(), module, MAKEINTRESOURCEW(resource_id)) != FALSE; }
bool Animation::Play(UINT repeat_count, UINT from, UINT to) noexcept { return IsWindow() && Animate_Play(GetHwnd(), from, to, repeat_count) != FALSE; }
Animation& Animation::Stop() noexcept { if (IsWindow()) Animate_Stop(GetHwnd()); return *this; }

}  // namespace mwtl
