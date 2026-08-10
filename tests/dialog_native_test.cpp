#include <mwtl/mwtl.h>

#include <windows.h>
#include <oleacc.h>

#include <cstdlib>
#include <stdexcept>
#include <thread>

namespace {

void PumpPendingMessages() {
    MSG message{};
    while (::PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE) != FALSE) {
        ::TranslateMessage(&message);
        ::DispatchMessageW(&message);
    }
}

HWND CreateOwner() {
    return ::CreateWindowExW(0, L"STATIC", L"Dialog owner", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                             CW_USEDEFAULT, 500, 350, nullptr, nullptr, ::GetModuleHandleW(nullptr),
                             nullptr);
}

}  // namespace

int main() {
    using namespace mwtl;
    using mwtl::operator""_dip;

    const HWND owner = CreateOwner();
    if (owner == nullptr) return 1;

    Label label;
    Dialog* dialog_pointer = nullptr;
    int custom_commands = 0;
    int close_attempts = 0;
    int destroyed = 0;
    Dialog dialog({
        .owner = owner,
        .title = L"Modeless lifecycle",
        .initial_client_size = {420.0_dip, 260.0_dip},
        .disable_owner_for_modeless = true,
        .callbacks = {
            .initialize =
                [&](HWND window) {
                    ControlHost ui{window};
                    ui.Add(label, {101}, L"Resizable modeless content", RectDip{});
                    return dialog_pointer->SetLayout(
                        Column().Margin(12.0_dip).Add(label, Stretch()));
                },
            .command =
                [&](HWND, WORD id, WORD) {
                    if (id != 200) return false;
                    ++custom_commands;
                    return true;
                },
            .close =
                [&](HWND) {
                    ++close_attempts;
                    return false;
                },
            .destroyed = [&] { ++destroyed; },
        },
    });
    dialog_pointer = &dialog;
    if (!dialog.CreateModeless() || !dialog.IsWindow() || !dialog.IsModeless() ||
        !dialog.IsOwnerThread() || ::IsWindowEnabled(owner) != FALSE)
        return 2;
    const HWND modeless = dialog.GetHwnd();
    wchar_t title[64]{};
    ::GetWindowTextW(modeless, title, 64);
    if (std::wstring_view(title) != L"Modeless lifecycle" || label.GetHwnd() == nullptr) return 3;
    RECT initial_client{};
    ::GetClientRect(modeless, &initial_client);
    const DpiContext modeless_dpi = DpiContext::FromWindow(modeless);
    if (initial_client.right != modeless_dpi.ToPixels(420.0_dip) ||
        initial_client.bottom != modeless_dpi.ToPixels(260.0_dip) ||
        ::SendMessageW(modeless, WM_GETFONT, 0, 0) == 0 ||
        ::SendMessageW(label.GetHwnd(), WM_GETFONT, 0, 0) !=
            ::SendMessageW(modeless, WM_GETFONT, 0, 0))
        return 29;
    ::SendMessageW(modeless, WM_COMMAND, MAKEWPARAM(200, BN_CLICKED), 0);
    if (custom_commands != 1) return 4;
    ::SendMessageW(modeless, WM_CLOSE, 0, 0);
    if (!dialog.IsWindow() || close_attempts != 1) return 5;

    RECT dialog_before{};
    RECT label_before{};
    ::GetWindowRect(modeless, &dialog_before);
    ::GetClientRect(label.GetHwnd(), &label_before);
    if (::SetWindowPos(modeless, nullptr, 0, 0, dialog_before.right - dialog_before.left + 100,
                       dialog_before.bottom - dialog_before.top + 70,
                       SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOZORDER) == FALSE)
        return 6;
    RECT label_after{};
    ::GetClientRect(label.GetHwnd(), &label_after);
    if (label_after.right <= label_before.right || label_after.bottom <= label_before.bottom)
        return 7;

    if (!dialog.Accept(42) || dialog.IsWindow() ||
        dialog.GetResult().status != DialogStatus::accepted ||
        dialog.GetResult().command_id != 42 || destroyed != 1 || ::IsWindowEnabled(owner) == FALSE)
        return 8;
    dialog.Close();
    if (destroyed != 1) return 9;

    bool modal_owner_disabled = false;
    Dialog* modal_pointer = nullptr;
    Dialog modal({
        .owner = owner,
        .title = L"Modal lifecycle",
        .callbacks =
            {
                .initialize =
                    [&](HWND) {
                        modal_owner_disabled = ::IsWindowEnabled(owner) == FALSE;
                        return modal_pointer->Accept(77);
                    },
            },
    });
    modal_pointer = &modal;
    const DialogResult modal_result = modal.ShowModal();
    if (!modal_result || modal_result.command_id != 77 || !modal_owner_disabled ||
        ::IsWindowEnabled(owner) == FALSE)
        return 10;

    Dialog* cancelled_modal_pointer = nullptr;
    Dialog cancelled_modal({
        .owner = owner,
        .callbacks = {.initialize = [&](HWND) { return cancelled_modal_pointer->Cancel(88); }},
    });
    cancelled_modal_pointer = &cancelled_modal;
    const DialogResult cancelled_modal_result = cancelled_modal.ShowModal();
    if (cancelled_modal_result.status != DialogStatus::cancelled ||
        cancelled_modal_result.command_id != 88 || ::IsWindowEnabled(owner) == FALSE)
        return 30;

    Dialog initialization_failure({
        .title = L"Initialization failure",
        .callbacks = {.initialize = [](HWND) { return false; }},
    });
    const DialogResult initialization_result = initialization_failure.ShowModal();
    if (initialization_result.status != DialogStatus::failed ||
        initialization_result.error != ERROR_FUNCTION_FAILED)
        return 11;

    Dialog throwing_initialization({
        .title = L"Throwing initialization",
        .callbacks = {.initialize = [](HWND) -> bool {
            throw std::runtime_error("initialization failure");
        }},
    });
    const DialogResult throwing_initialization_result = throwing_initialization.ShowModal();
    if (throwing_initialization_result.status != DialogStatus::failed ||
        throwing_initialization_result.error != ERROR_UNHANDLED_EXCEPTION ||
        !throwing_initialization_result.callback_exception)
        return 12;

    Dialog throwing_command({
        .title = L"Throwing command",
        .callbacks = {
            .initialize = [](HWND) { return true; },
            .command = [](HWND, WORD, WORD) -> bool {
                throw std::runtime_error("command failure");
            },
        },
    });
    if (!throwing_command.CreateModeless()) return 13;
    ::SendMessageW(throwing_command.GetHwnd(), WM_COMMAND, MAKEWPARAM(300, BN_CLICKED), 0);
    if (throwing_command.IsWindow() ||
        throwing_command.GetResult().status != DialogStatus::failed ||
        !throwing_command.GetResult().callback_exception)
        return 14;

    Dialog ordinary_modeless({.owner = owner, .title = L"Ordinary modeless"});
    if (!ordinary_modeless.CreateModeless() || ::IsWindowEnabled(owner) == FALSE) return 15;
    if (ordinary_modeless.CreateModeless() || ::GetLastError() != ERROR_INVALID_STATE) return 16;
    if (!ordinary_modeless.Cancel(9) ||
        ordinary_modeless.GetResult().status != DialogStatus::cancelled ||
        ordinary_modeless.GetResult().command_id != 9)
        return 17;

    Dialog invalid_owner({.owner = reinterpret_cast<HWND>(1), .disable_owner_for_modeless = true});
    if (invalid_owner.CreateModeless() || ::GetLastError() != ERROR_INVALID_WINDOW_HANDLE)
        return 18;

    bool destroyed_threw = false;
    Dialog throwing_destroyed({
        .callbacks = {.destroyed =
                          [&] {
                              destroyed_threw = true;
                              throw std::runtime_error("destroyed failure");
                          }},
    });
    if (!throwing_destroyed.CreateModeless() || !throwing_destroyed.Accept()) return 19;
    if (!destroyed_threw || throwing_destroyed.GetResult().status != DialogStatus::failed ||
        throwing_destroyed.GetResult().error != ERROR_UNHANDLED_EXCEPTION ||
        !throwing_destroyed.GetResult().callback_exception)
        return 20;

    try {
        Dialog invalid_size({.initial_client_size = {0.0_dip, 100.0_dip}});
        return 21;
    } catch (const std::invalid_argument&) {
    }

    Dialog move_target({.title = L"Move target"});
    Dialog move_source({.title = L"Move source"});
    if (!move_target.CreateModeless() || !move_source.CreateModeless()) return 22;
    const HWND old_target = move_target.GetHwnd();
    const HWND moved_window = move_source.GetHwnd();
    move_target = std::move(move_source);
    if (::IsWindow(old_target) != FALSE || move_target.GetHwnd() != moved_window ||
        move_source.IsWindow())
        return 23;
    move_target.Close();

    Button first, second;
    Dialog* keyboard_pointer = nullptr;
    Dialog keyboard({
        .title = L"Keyboard and accessibility",
        .callbacks = {.initialize =
                          [&](HWND window) {
                              ControlHost ui{window};
                              ui.Add(first, {401}, L"First", {});
                              ui.Add(second, {402}, L"Second", {});
                              SetAccessibleName(second.GetHwnd(), L"Second dialog action");
                              return keyboard_pointer->SetLayout(Row()
                                                                     .Margin(12.0_dip)
                                                                     .Gap(8.0_dip)
                                                                     .Add(first, Stretch())
                                                                     .Add(second, Stretch()));
                          }},
    });
    keyboard_pointer = &keyboard;
    if (!keyboard.CreateModeless()) return 31;
    ::SetFocus(first.GetHwnd());
    MSG tab{};
    tab.hwnd = first.GetHwnd();
    tab.message = WM_KEYDOWN;
    tab.wParam = VK_TAB;
    if (::IsDialogMessageW(keyboard.GetHwnd(), &tab) == FALSE || ::GetFocus() != second.GetHwnd())
        return 32;
    IAccessible* accessible = nullptr;
    if (FAILED(::AccessibleObjectFromWindow(second.GetHwnd(), static_cast<DWORD>(OBJID_CLIENT),
                                            IID_IAccessible,
                                            reinterpret_cast<void**>(&accessible))) ||
        accessible == nullptr)
        return 33;
    VARIANT self{};
    self.vt = VT_I4;
    self.lVal = CHILDID_SELF;
    BSTR accessible_name = nullptr;
    const HRESULT name_result = accessible->get_accName(self, &accessible_name);
    accessible->Release();
    const bool accessible_name_matches =
        SUCCEEDED(name_result) && accessible_name != nullptr &&
        std::wstring_view(accessible_name) == L"Second dialog action";
    ::SysFreeString(accessible_name);
    if (!accessible_name_matches) return 34;
    ::SendMessageW(keyboard.GetHwnd(), WM_SETTINGCHANGE, 0, 0);
    ::SendMessageW(keyboard.GetHwnd(), WM_THEMECHANGED, 0, 0);
    keyboard.Close();

    Dialog cross_thread({.title = L"Cross-thread close"});
    if (!cross_thread.CreateModeless()) return 24;
    const HWND cross_thread_window = cross_thread.GetHwnd();
    std::thread closer([value = std::move(cross_thread)]() mutable {
        if (value.IsOwnerThread()) std::terminate();
        value.Close();
    });
    closer.join();
    PumpPendingMessages();
    if (::IsWindow(cross_thread_window) != FALSE) return 25;

    const DWORD user_before = ::GetGuiResources(::GetCurrentProcess(), GR_USEROBJECTS);
    const DWORD gdi_before = ::GetGuiResources(::GetCurrentProcess(), GR_GDIOBJECTS);
    for (int iteration = 0; iteration < 40; ++iteration) {
        Dialog stress({.title = L"Dialog stress"});
        if (!stress.CreateModeless()) return 26;
        stress.Close();
        if (stress.IsWindow()) return 27;
    }
    const DWORD user_after = ::GetGuiResources(::GetCurrentProcess(), GR_USEROBJECTS);
    const DWORD gdi_after = ::GetGuiResources(::GetCurrentProcess(), GR_GDIOBJECTS);
    if (user_after > user_before + 8 || gdi_after > gdi_before + 8) return 28;

    ::DestroyWindow(owner);
    return EXIT_SUCCESS;
}
