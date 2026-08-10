#include <mwfl/mwfl.h>

#include <windows.h>

#include <cstdlib>
#include <string>
#include <thread>

namespace {

constexpr UINT kTrayMessage = WM_APP + 73;
constexpr GUID kPrimaryIdentity{
    0x528cfcb1, 0xca9c, 0x49dc, {0xb0, 0xd5, 0x81, 0xba, 0x8f, 0x0b, 0x64, 0x3f}};
constexpr GUID kSecondaryIdentity{
    0x8464cc6e, 0x551f, 0x4771, {0xb7, 0x32, 0x95, 0xe0, 0x18, 0xf7, 0xdd, 0x39}};

HWND CreateOwner() {
    return ::CreateWindowExW(0, L"STATIC", L"Tray icon test owner", WS_OVERLAPPEDWINDOW,
                             CW_USEDEFAULT, CW_USEDEFAULT, 400, 240, nullptr, nullptr,
                             ::GetModuleHandleW(nullptr), nullptr);
}

mwfl::TrayIconOptions Options(HWND owner, UINT id, GUID identity, std::wstring tooltip) {
    return {.owner = owner,
            .id = id,
            .callback_message = kTrayMessage,
            .identity = identity,
            .icon = ::LoadIconW(nullptr, IDI_APPLICATION),
            .tooltip = std::move(tooltip)};
}

}  // namespace

int main() {
    using namespace mwfl;

    const HWND owner = CreateOwner();
    if (owner == nullptr) return 1;

    TrayIcon invalid;
    if (invalid.Add({}) || ::GetLastError() != ERROR_INVALID_PARAMETER) return 2;
    auto invalid_guid = Options(owner, 1, GUID_NULL, L"Invalid GUID");
    if (invalid.Add(std::move(invalid_guid)) || ::GetLastError() != ERROR_INVALID_PARAMETER)
        return 3;
    auto invalid_message = Options(owner, 1, kPrimaryIdentity, L"Invalid message");
    invalid_message.callback_message = WM_USER + 1;
    if (invalid.Add(std::move(invalid_message)) || ::GetLastError() != ERROR_INVALID_PARAMETER)
        return 4;

    TrayIcon icon;
    if (!icon.Add(Options(owner, 17, kPrimaryIdentity, L"mwfl tray test")) || !icon.IsAdded() ||
        icon.GetState() != TrayIconState::added || !icon.IsOwnerThread())
        return 5;
    if (icon.Add(Options(owner, 18, kSecondaryIdentity, L"Duplicate")) ||
        ::GetLastError() != ERROR_ALREADY_EXISTS)
        return 6;

    const LPARAM keyboard_lparam = MAKELPARAM(NIN_KEYSELECT, 17);
    const WPARAM keyboard_wparam = MAKEWPARAM(120, 240);
    const auto keyboard = icon.Decode({kTrayMessage, keyboard_wparam, keyboard_lparam});
    if (!keyboard || keyboard->kind != TrayIconEventKind::primary_activate || !keyboard->keyboard ||
        keyboard->screen_position.x != 120 || keyboard->screen_position.y != 240)
        return 7;
    const auto context =
        icon.Decode({kTrayMessage, MAKEWPARAM(321, 123), MAKELPARAM(WM_CONTEXTMENU, 17)});
    if (!context || context->kind != TrayIconEventKind::context_menu || context->keyboard) return 8;
    const auto unknown = icon.Decode({kTrayMessage, 0, MAKELPARAM(WM_MOUSEMOVE, 17)});
    if (!unknown || unknown->kind != TrayIconEventKind::unknown ||
        icon.Decode({kTrayMessage, 0, MAKELPARAM(NIN_SELECT, 19)}) ||
        icon.Decode({kTrayMessage + 1, 0, MAKELPARAM(NIN_SELECT, 17)}))
        return 9;

    std::wstring oversized_tip(128, L'x');
    if (icon.UpdateTooltip(oversized_tip) || ::GetLastError() != ERROR_BUFFER_OVERFLOW ||
        icon.GetOptions().tooltip != L"mwfl tray test")
        return 10;
    if (!icon.UpdateTooltip(L"mwfl tray updated") ||
        icon.GetOptions().tooltip != L"mwfl tray updated" || !icon.SetVisible(false) ||
        !icon.GetOptions().hidden || !icon.SetVisible(true) || icon.GetOptions().hidden ||
        !icon.UpdateIcon(::LoadIconW(nullptr, IDI_INFORMATION)))
        return 11;
    if (!icon.ShowNotification({.title = L"mwfl TrayIcon",
                                .text = L"Native notification protocol test",
                                .respect_quiet_time = true}))
        return 12;

    WindowMessage taskbar_created{TrayIcon::GetTaskbarCreatedMessage(), 0, 0};
    if (taskbar_created.id == 0 || !icon.IsTaskbarCreated(taskbar_created) ||
        icon.IsTaskbarCreated({WM_NULL, 0, 0}) || !icon.Recreate() || !icon.IsAdded())
        return 13;

    bool rejected_wrong_thread = false;
    std::thread wrong_thread([&] {
        rejected_wrong_thread =
            !icon.UpdateTooltip(L"wrong thread") && ::GetLastError() == ERROR_INVALID_THREAD_ID;
    });
    wrong_thread.join();
    if (!rejected_wrong_thread || icon.GetOptions().tooltip != L"mwfl tray updated") return 14;

    TrayIcon replacement;
    if (!replacement.Add(Options(owner, 18, kSecondaryIdentity, L"Replacement"))) return 15;
    replacement = std::move(icon);
    if (!replacement.IsAdded() || icon.GetState() != TrayIconState::detached ||
        replacement.GetOptions().id != 17)
        return 16;
    replacement.Remove();
    replacement.Remove();
    if (replacement.GetState() != TrayIconState::detached ||
        replacement.UpdateTooltip(L"after remove") || ::GetLastError() != ERROR_INVALID_STATE)
        return 17;

    const HWND short_lived_owner = CreateOwner();
    if (short_lived_owner == nullptr) return 18;
    TrayIcon owner_race;
    if (!owner_race.Add(Options(short_lived_owner, 19, kSecondaryIdentity, L"Owner race")))
        return 19;
    ::DestroyWindow(short_lived_owner);
    owner_race.Remove();
    if (owner_race.GetState() != TrayIconState::detached) return 20;

    const DWORD user_before = ::GetGuiResources(::GetCurrentProcess(), GR_USEROBJECTS);
    const DWORD gdi_before = ::GetGuiResources(::GetCurrentProcess(), GR_GDIOBJECTS);
    for (UINT iteration = 0; iteration < 40; ++iteration) {
        GUID identity = kPrimaryIdentity;
        identity.Data1 += iteration + 1;
        TrayIcon stress;
        if (!stress.Add(Options(owner, 100 + iteration, identity, L"Tray stress"))) return 21;
        if (!stress.UpdateTooltip(L"Tray stress updated") || !stress.SetVisible(false) ||
            !stress.SetVisible(true) || !stress.Recreate())
            return 22;
        stress.Remove();
    }
    const DWORD user_after = ::GetGuiResources(::GetCurrentProcess(), GR_USEROBJECTS);
    const DWORD gdi_after = ::GetGuiResources(::GetCurrentProcess(), GR_GDIOBJECTS);
    if (user_after > user_before + 8 || gdi_after > gdi_before + 8) return 23;

    ::DestroyWindow(owner);
    return EXIT_SUCCESS;
}
