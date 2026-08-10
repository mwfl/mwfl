#include <mwfl/mwfl.h>

#include <windows.h>
#include <oleacc.h>

#include <climits>
#include <cstdlib>
#include <string>
#include <string_view>

namespace {

HWND CreateOwner() {
    return ::CreateWindowExW(0, L"STATIC", L"Resource test owner", WS_OVERLAPPEDWINDOW,
                             CW_USEDEFAULT, CW_USEDEFAULT, 480, 320, nullptr, nullptr,
                             ::GetModuleHandleW(nullptr), nullptr);
}

void CALLBACK CancelPopup(HWND, UINT, UINT_PTR timer, DWORD) {
    ::KillTimer(nullptr, timer);
    ::EndMenu();
}

bool AccessibleNameEquals(HWND window, std::wstring_view expected) {
    IAccessible* accessible = nullptr;
    if (FAILED(::AccessibleObjectFromWindow(window, static_cast<DWORD>(OBJID_CLIENT),
                                            IID_IAccessible,
                                            reinterpret_cast<void**>(&accessible))) ||
        accessible == nullptr)
        return false;
    VARIANT self{};
    self.vt = VT_I4;
    self.lVal = CHILDID_SELF;
    BSTR name = nullptr;
    const HRESULT result = accessible->get_accName(self, &name);
    accessible->Release();
    const bool matches = SUCCEEDED(result) && name != nullptr && expected == name;
    ::SysFreeString(name);
    return matches;
}

}  // namespace

int main() {
    using namespace mwfl;

    const HWND owner = CreateOwner();
    if (owner == nullptr) return 1;
    const HWND button = ::CreateWindowExW(
        0, L"BUTTON", L"Resource action", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 10, 10, 180, 32,
        owner, reinterpret_cast<HMENU>(101), ::GetModuleHandleW(nullptr), nullptr);
    if (button == nullptr || !SetAccessibleName(button, L"Resource action with details")) return 2;

    Tooltip tooltip;
    if (tooltip.SetActive(true) || ::GetLastError() != ERROR_INVALID_STATE) return 29;
    if (!tooltip.Create(owner, {.balloon = true, .close_button = true, .max_width = 280}) ||
        !tooltip.IsWindow() || tooltip.GetOwner() != owner ||
        (static_cast<DWORD>(::GetWindowLongPtrW(tooltip.GetHwnd(), GWL_STYLE)) &
         (TTS_BALLOON | TTS_CLOSE)) != (TTS_BALLOON | TTS_CLOSE) ||
        ::SendMessageW(tooltip.GetHwnd(), TTM_GETMAXTIPWIDTH, 0, 0) != 280)
        return 3;
    if (!tooltip.SetTitle(TTI_INFO, L"Resource details") ||
        !tooltip.SetDelayTime(TTDT_INITIAL, 25) ||
        ::SendMessageW(tooltip.GetHwnd(), TTM_GETDELAYTIME, TTDT_INITIAL, 0) != 25 ||
        !tooltip.AddTool(button, L"Initial resource tooltip") || !tooltip.HasTool(button) ||
        tooltip.GetToolCount() != 1 ||
        ::SendMessageW(tooltip.GetHwnd(), TTM_GETTOOLCOUNT, 0, 0) != 1)
        return 4;
    if (tooltip.SetTitle(TTI_ERROR_LARGE + 1, L"Invalid icon") ||
        tooltip.SetDelayTime(TTDT_INITIAL, USHRT_MAX + 1))
        return 27;
    if (tooltip.AddTool(button, L"Duplicate") || ::GetLastError() != ERROR_INVALID_PARAMETER)
        return 5;

    wchar_t tooltip_text[128]{};
    TOOLINFOW native_tool{};
    native_tool.cbSize = sizeof(native_tool);
    native_tool.hwnd = owner;
    native_tool.uId = reinterpret_cast<UINT_PTR>(button);
    native_tool.lpszText = tooltip_text;
    ::SendMessageW(tooltip.GetHwnd(), TTM_GETTEXTW, 128, reinterpret_cast<LPARAM>(&native_tool));
    if (std::wstring_view(tooltip_text) != L"Initial resource tooltip") return 6;

    if (!tooltip.UpdateTool(button, L"Updated resource tooltip")) return 7;
    tooltip_text[0] = L'\0';
    ::SendMessageW(tooltip.GetHwnd(), TTM_GETTEXTW, 128, reinterpret_cast<LPARAM>(&native_tool));
    if (std::wstring_view(tooltip_text) != L"Updated resource tooltip" ||
        !AccessibleNameEquals(button, L"Resource action with details"))
        return 8;
    MSG relay{button, WM_MOUSEMOVE, 0, MAKELPARAM(2, 2), 0, {2, 2}};
    if (!tooltip.SetActive(false) || !tooltip.SetActive(true) || !tooltip.RelayEvent(relay))
        return 9;
    tooltip.Update();
    tooltip.Pop();
    if (!tooltip.RemoveTool(button) || tooltip.HasTool(button) || tooltip.GetToolCount() != 0 ||
        tooltip.RemoveTool(button) || ::GetLastError() != ERROR_NOT_FOUND)
        return 10;

    Tooltip moved_tooltip = std::move(tooltip);
    if (!moved_tooltip.IsWindow() || tooltip.IsWindow()) return 11;

    ImageList images;
    if (images.AddIcon(::LoadIconW(nullptr, IDI_APPLICATION)) >= 0 ||
        ::GetLastError() != ERROR_INVALID_STATE || images.Create(0, 16) ||
        ::GetLastError() != ERROR_INVALID_PARAMETER ||
        !images.Create(20, 24, ILC_COLOR32 | ILC_MASK, 2, 2) || !images.IsCreated())
        return 12;
    SIZE image_size{};
    if (!images.GetIconSize(image_size) || image_size.cx != 20 || image_size.cy != 24) return 13;
    const int first = images.AddIcon(::LoadIconW(nullptr, IDI_APPLICATION));
    const int second = images.AddIcon(::LoadIconW(nullptr, IDI_INFORMATION));
    if (first != 0 || second != 1 || ImageList_GetImageCount(images.GetHandle()) != 2 ||
        !images.ReplaceIcon(first, ::LoadIconW(nullptr, IDI_WARNING)) ||
        !images.SetOverlayImage(first, 1) || !images.SetBackgroundColor(RGB(1, 2, 3)) ||
        images.GetBackgroundColor() != RGB(1, 2, 3))
        return 14;
    if (images.ReplaceIcon(8, ::LoadIconW(nullptr, IDI_WARNING)) ||
        ::GetLastError() != ERROR_INVALID_PARAMETER || !images.Remove(second) ||
        images.GetCount() != 1 || !images.RemoveAll() || images.GetCount() != 0)
        return 15;
    ImageList moved_images = std::move(images);
    if (!moved_images.IsCreated() || images.IsCreated()) return 16;

    Menu popup;
    Command command({701}, L"Initial command");
    command.SetChecked(true).SetEnabled(false);
    const PopupMenuResult missing_popup = popup.TrackResult(owner, {0, 0});
    if (missing_popup.status != PopupMenuStatus::failed ||
        missing_popup.error != ERROR_INVALID_MENU_HANDLE)
        return 30;
    if (!popup.CreatePopup() || popup.GetKind() != MenuKind::popup || !popup.IsOwned() ||
        popup.AppendCommand(0, L"Invalid") || !popup.AppendCommand(command))
        return 17;
    MENUITEMINFOW menu_item{};
    menu_item.cbSize = sizeof(menu_item);
    menu_item.fMask = MIIM_STATE;
    if (::GetMenuItemInfoW(popup.GetHandle(), 701, FALSE, &menu_item) == FALSE ||
        (menu_item.fState & MFS_CHECKED) == 0 || (menu_item.fState & MFS_DISABLED) == 0)
        return 18;
    command.SetText(L"Updated command").SetChecked(false).SetEnabled(true);
    if (!popup.UpdateCommand(command)) return 19;
    wchar_t menu_text[64]{};
    if (::GetMenuStringW(popup.GetHandle(), 701, menu_text, 64, MF_BYCOMMAND) == 0 ||
        std::wstring_view(menu_text) != L"Updated command")
        return 20;
    const PopupMenuResult invalid_owner = popup.TrackResult(nullptr, {0, 0});
    if (invalid_owner.status != PopupMenuStatus::failed ||
        invalid_owner.error != ERROR_INVALID_WINDOW_HANDLE)
        return 21;
    if (::SetTimer(nullptr, 0, 25, CancelPopup) == 0) return 22;
    const PopupMenuResult cancelled = popup.TrackResult(owner, {20, 20}, TPM_NONOTIFY);
    if (!cancelled.Cancelled() || cancelled.command != 0 || cancelled.error != ERROR_SUCCESS)
        return 23;

    Menu bar, submenu;
    if (popup.AttachToWindow(owner)) return 28;
    if (!bar.Create() || !submenu.CreatePopup() || !submenu.AppendCommand(702, L"Child") ||
        !bar.AppendSubmenu(std::move(submenu), L"Commands") ||
        submenu.GetKind() != MenuKind::none || submenu.IsOwned() || !bar.AttachToWindow(owner) ||
        bar.GetKind() != MenuKind::attached || bar.IsOwned())
        return 24;

    const DWORD user_before = ::GetGuiResources(::GetCurrentProcess(), GR_USEROBJECTS);
    const DWORD gdi_before = ::GetGuiResources(::GetCurrentProcess(), GR_GDIOBJECTS);
    for (int iteration = 0; iteration < 40; ++iteration) {
        Tooltip stress_tip;
        ImageList stress_images;
        Menu stress_menu;
        if (!stress_tip.Create(owner) || !stress_tip.AddTool(button, L"Stress tooltip") ||
            !stress_images.Create(16, 16) ||
            stress_images.AddIcon(::LoadIconW(nullptr, IDI_APPLICATION)) < 0 ||
            !stress_menu.CreatePopup() || !stress_menu.AppendCommand(800, L"Stress command"))
            return 25;
    }
    const DWORD user_after = ::GetGuiResources(::GetCurrentProcess(), GR_USEROBJECTS);
    const DWORD gdi_after = ::GetGuiResources(::GetCurrentProcess(), GR_GDIOBJECTS);
    if (user_after > user_before + 8 || gdi_after > gdi_before + 8) return 26;

    moved_tooltip.Destroy();
    moved_images.Reset();
    bar.Reset();
    ::DestroyWindow(owner);
    return EXIT_SUCCESS;
}
