#include <mwfl/mwfl.h>

#include <commctrl.h>
#include <oleacc.h>

#include <string_view>

namespace {

struct ForwardState {
    HWND source = nullptr;
    int notify_count = 0;
    int command_count = 0;
    int context_count = 0;
};

LRESULT CALLBACK ParentSubclass(HWND window, UINT message, WPARAM wparam, LPARAM lparam,
                                UINT_PTR subclass_id, DWORD_PTR reference) {
    auto* state = reinterpret_cast<ForwardState*>(reference);
    if (message == WM_NOTIFY && state != nullptr) {
        const auto* header = reinterpret_cast<const NMHDR*>(lparam);
        state->source = header == nullptr ? nullptr : header->hwndFrom;
        ++state->notify_count;
        return 301;
    }
    if (message == WM_COMMAND && state != nullptr) {
        state->source = reinterpret_cast<HWND>(lparam);
        ++state->command_count;
        return 302;
    }
    if (message == WM_CONTEXTMENU && state != nullptr) {
        state->source = reinterpret_cast<HWND>(wparam);
        ++state->context_count;
        return 303;
    }
    if (message == WM_NCDESTROY)
        ::RemoveWindowSubclass(window, ParentSubclass, subclass_id);
    return ::DefSubclassProc(window, message, wparam, lparam);
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
    const bool matches = SUCCEEDED(accessible->get_accName(self, &name)) && name != nullptr &&
                         std::wstring_view{name, ::SysStringLen(name)} == expected;
    if (name != nullptr) ::SysFreeString(name);
    accessible->Release();
    return matches;
}

DWORD GuiResources() noexcept {
    return ::GetGuiResources(::GetCurrentProcess(), GR_GDIOBJECTS) +
           ::GetGuiResources(::GetCurrentProcess(), GR_USEROBJECTS);
}

}  // namespace

int main() {
    using namespace mwfl;
    using mwfl::operator""_dip;

    const HWND parent = ::CreateWindowExW(0, L"STATIC", L"Native host test parent",
                                          WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 640, 480,
                                          nullptr, nullptr, ::GetModuleHandleW(nullptr), nullptr);
    if (parent == nullptr) return 1;
    ForwardState forwarded;
    if (::SetWindowSubclass(parent, ParentSubclass, 1,
                            reinterpret_cast<DWORD_PTR>(&forwarded)) == FALSE)
        return 2;

    NativeHost host;
    if (!host.Create(parent, {801}, {10.0_dip, 10.0_dip, 400.0_dip, 240.0_dip}) ||
        host.GetState() != NativeHostState::ready || host.GetChild() != nullptr ||
        (::GetWindowLongPtrW(host.GetHwnd(), GWL_STYLE) & WS_TABSTOP) == 0 ||
        (::GetWindowLongPtrW(host.GetHwnd(), GWL_EXSTYLE) & WS_EX_CONTROLPARENT) == 0 ||
        !AccessibleNameEquals(host.GetHwnd(), L"Native content host"))
        return 3;

    const HWND unrelated = ::CreateWindowExW(0, L"STATIC", L"unrelated", WS_CHILD,
                                              0, 0, 10, 10, parent, nullptr,
                                              ::GetModuleHandleW(nullptr), nullptr);
    if (host.Attach(nullptr) || ::GetLastError() != ERROR_INVALID_PARAMETER ||
        host.Attach(unrelated) || ::GetLastError() != ERROR_INVALID_PARAMETER)
        return 4;

    const HWND child = ::CreateWindowExW(0, L"STATIC", L"third-party surface",
                                         WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 1, 1,
                                         host.GetHwnd(),
                                         reinterpret_cast<HMENU>(static_cast<INT_PTR>(901)),
                                         ::GetModuleHandleW(nullptr), nullptr);
    const HWND nested = ::CreateWindowExW(0, L"BUTTON", L"nested action",
                                          WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 20, 20,
                                          child,
                                          reinterpret_cast<HMENU>(static_cast<INT_PTR>(902)),
                                          ::GetModuleHandleW(nullptr), nullptr);
    if (child == nullptr || nested == nullptr || !host.Attach(child) ||
        host.GetState() != NativeHostState::attached || host.GetChild() != child ||
        host.Attach(child) || ::GetLastError() != ERROR_INVALID_STATE)
        return 5;

    RECT host_client{};
    RECT child_bounds{};
    ::GetClientRect(host.GetHwnd(), &host_client);
    ::GetWindowRect(child, &child_bounds);
    ::MapWindowPoints(nullptr, host.GetHwnd(), reinterpret_cast<POINT*>(&child_bounds), 2);
    if (child_bounds.left != 0 || child_bounds.top != 0 ||
        child_bounds.right != host_client.right || child_bounds.bottom != host_client.bottom)
        return 6;

    ::SetFocus(host.GetHwnd());
    if (::GetFocus() != child) return 7;

    NMHDR notification{nested, 902, NM_CLICK};
    if (::SendMessageW(host.GetHwnd(), WM_NOTIFY, notification.idFrom,
                       reinterpret_cast<LPARAM>(&notification)) != 301 ||
        forwarded.notify_count != 1 || forwarded.source != nested)
        return 8;
    if (::SendMessageW(host.GetHwnd(), WM_COMMAND, MAKEWPARAM(902, BN_CLICKED),
                       reinterpret_cast<LPARAM>(nested)) != 302 ||
        forwarded.command_count != 1 || forwarded.source != nested)
        return 9;
    if (::SendMessageW(host.GetHwnd(), WM_CONTEXTMENU, reinterpret_cast<WPARAM>(nested),
                       MAKELPARAM(20, 30)) != 303 ||
        forwarded.context_count != 1 || forwarded.source != nested)
        return 10;

    ::SendMessageW(host.GetHwnd(), WM_THEMECHANGED, 0, 0);
    ::SendMessageW(host.GetHwnd(), WM_SETTINGCHANGE, 0, 0);
    if (!host.SetBounds({20.0_dip, 20.0_dip, 360.0_dip, 200.0_dip})) return 11;

    if (host.Detach() != child || host.GetState() != NativeHostState::ready ||
        ::IsWindow(child) == FALSE || host.Detach() != nullptr ||
        ::GetLastError() != ERROR_INVALID_STATE)
        return 12;
    ::DestroyWindow(child);

    const HWND transient = ::CreateWindowExW(0, L"STATIC", L"transient", WS_CHILD | WS_VISIBLE,
                                              0, 0, 1, 1, host.GetHwnd(),
                                              reinterpret_cast<HMENU>(static_cast<INT_PTR>(903)),
                                              ::GetModuleHandleW(nullptr), nullptr);
    if (transient == nullptr || !host.Attach(transient)) return 13;
    ::DestroyWindow(transient);
    if (host.GetState() != NativeHostState::ready || host.GetChild() != nullptr ||
        !host.Arrange())
        return 14;

    const DWORD resources_before = GuiResources();
    for (int iteration = 0; iteration < 100; ++iteration) {
        NativeHost fixture;
        if (!fixture.Create(parent, {1000 + iteration},
                            {0.0_dip, 0.0_dip, 80.0_dip, 60.0_dip}))
            return 15;
        const HWND surface = ::CreateWindowExW(0, L"STATIC", L"surface",
                                               WS_CHILD | WS_VISIBLE, 0, 0, 1, 1,
                                               fixture.GetHwnd(),
                                               reinterpret_cast<HMENU>(
                                                   static_cast<INT_PTR>(2000 + iteration)),
                                               ::GetModuleHandleW(nullptr), nullptr);
        if (surface == nullptr || !fixture.Attach(surface)) return 16;
    }
    if (GuiResources() > resources_before + 8) return 17;

    const HWND owned_by_parent = ::CreateWindowExW(0, L"STATIC", L"destroyed with host",
                                                    WS_CHILD | WS_VISIBLE, 0, 0, 1, 1,
                                                    host.GetHwnd(),
                                                    reinterpret_cast<HMENU>(
                                                        static_cast<INT_PTR>(904)),
                                                    ::GetModuleHandleW(nullptr), nullptr);
    if (owned_by_parent == nullptr || !host.Attach(owned_by_parent)) return 18;
    host.Destroy();
    if (host.GetState() != NativeHostState::destroyed ||
        ::IsWindow(owned_by_parent) != FALSE)
        return 19;

    ::DestroyWindow(unrelated);
    ::DestroyWindow(parent);
    return 0;
}
