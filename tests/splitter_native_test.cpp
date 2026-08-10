#include <mwfl/splitter.h>

#include <oleacc.h>
#include <wil/resource.h>

#include <cmath>
#include <cstdlib>
#include <string>

namespace {

struct NotificationState {
    int count = 0;
    mwfl::Dip position{};
    HWND forwarded_source = nullptr;
    int command_count = 0;
    int context_count = 0;
};

LRESULT CALLBACK ParentSubclass(HWND window, UINT message, WPARAM wparam, LPARAM lparam,
                                UINT_PTR subclass_id, DWORD_PTR reference) noexcept {
    auto* state = reinterpret_cast<NotificationState*>(reference);
    if (message == WM_NOTIFY && state != nullptr) {
        const auto* header = reinterpret_cast<const NMHDR*>(lparam);
        if (header != nullptr && header->code == mwfl::kSplitterPositionChanged) {
            const auto* notification = reinterpret_cast<const mwfl::SplitterNotification*>(lparam);
            ++state->count;
            state->position = notification->position;
            return 0;
        }
        if (header != nullptr && header->code == NM_CLICK) {
            state->forwarded_source = header->hwndFrom;
            return 123;
        }
    }
    if (message == WM_COMMAND && state != nullptr) {
        ++state->command_count;
        state->forwarded_source = reinterpret_cast<HWND>(lparam);
        return 124;
    }
    if (message == WM_CONTEXTMENU && state != nullptr) {
        ++state->context_count;
        state->forwarded_source = reinterpret_cast<HWND>(wparam);
        return 125;
    }
    if (message == WM_NCDESTROY) {
        ::RemoveWindowSubclass(window, ParentSubclass, subclass_id);
    }
    return ::DefSubclassProc(window, message, wparam, lparam);
}

RECT ChildBounds(HWND parent, HWND child) noexcept {
    RECT bounds{};
    ::GetWindowRect(child, &bounds);
    ::MapWindowPoints(nullptr, parent, reinterpret_cast<POINT*>(&bounds), 2);
    return bounds;
}

bool Near(int actual, int expected, int tolerance = 1) noexcept {
    return std::abs(actual - expected) <= tolerance;
}

DWORD GuiResources() noexcept {
    return ::GetGuiResources(::GetCurrentProcess(), GR_GDIOBJECTS) +
           ::GetGuiResources(::GetCurrentProcess(), GR_USEROBJECTS);
}

}  // namespace

int main() {
    using namespace mwfl;

    const HWND parent =
        ::CreateWindowExW(0, L"STATIC", L"splitter test parent", WS_OVERLAPPED, 0, 0, 640, 480,
                          nullptr, nullptr, ::GetModuleHandleW(nullptr), nullptr);
    if (parent == nullptr) return 1;
    const auto destroy_parent = wil::scope_exit([&] { ::DestroyWindow(parent); });

    NotificationState notifications;
    if (::SetWindowSubclass(parent, ParentSubclass, 1,
                            reinterpret_cast<DWORD_PTR>(&notifications)) == FALSE)
        return 2;

    Splitter splitter;
    if (!splitter.Create(
            parent, ControlId{201}, RectDip(Dip(0), Dip(0), Dip(400), Dip(200)),
            {.constraints = {Dip(100), Dip(80), Dip(6)}, .initial_position = Dip(120)}))
        return 3;
    if ((::GetWindowLongPtrW(splitter.GetHwnd(), GWL_STYLE) & WS_TABSTOP) == 0 ||
        ::SendMessageW(splitter.GetHwnd(), WM_GETDLGCODE, 0, 0) != DLGC_WANTARROWS) {
        return 4;
    }

    wchar_t accessible_text[32]{};
    if (::GetWindowTextW(splitter.GetHwnd(), accessible_text, 32) <= 0 ||
        std::wstring(accessible_text) != L"Pane splitter")
        return 5;
    IAccessible* accessible = nullptr;
    if (FAILED(::AccessibleObjectFromWindow(splitter.GetHwnd(), static_cast<DWORD>(OBJID_CLIENT),
                                            IID_IAccessible,
                                            reinterpret_cast<void**>(&accessible))) ||
        accessible == nullptr)
        return 6;
    BSTR name = nullptr;
    VARIANT self{};
    self.vt = VT_I4;
    self.lVal = CHILDID_SELF;
    const HRESULT name_result = accessible->get_accName(self, &name);
    VARIANT role{};
    VARIANT state{};
    const HRESULT role_result = accessible->get_accRole(self, &role);
    const HRESULT state_result = accessible->get_accState(self, &state);
    accessible->Release();
    const bool named =
        SUCCEEDED(name_result) && name != nullptr && std::wstring(name) == L"Pane splitter";
    ::SysFreeString(name);
    const bool role_matches =
        SUCCEEDED(role_result) && role.vt == VT_I4 && role.lVal == ROLE_SYSTEM_SLIDER;
    const bool state_matches =
        SUCCEEDED(state_result) && state.vt == VT_I4 && (state.lVal & STATE_SYSTEM_FOCUSABLE) != 0;
    ::VariantClear(&role);
    ::VariantClear(&state);
    if (!named) return 70;
    if (!role_matches) return 71;
    if (!state_matches) return 72;

    const auto pane = [&](const wchar_t* text) {
        return ::CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE, 0, 0, 1, 1,
                                 splitter.GetHwnd(), nullptr, ::GetModuleHandleW(nullptr), nullptr);
    };
    const HWND first = pane(L"first pane");
    const HWND second = pane(L"second pane");
    if (first == nullptr || second == nullptr || !splitter.AttachPanes(first, second)) {
        return 8;
    }

    NMHDR child_notification{first, 901, NM_CLICK};
    if (::SendMessageW(splitter.GetHwnd(), WM_NOTIFY, child_notification.idFrom,
                       reinterpret_cast<LPARAM>(&child_notification)) != 123 ||
        notifications.forwarded_source != first)
        return 80;
    if (::SendMessageW(splitter.GetHwnd(), WM_COMMAND, MAKEWPARAM(902, BN_CLICKED),
                       reinterpret_cast<LPARAM>(second)) != 124 ||
        notifications.command_count != 1 || notifications.forwarded_source != second)
        return 81;
    if (::SendMessageW(splitter.GetHwnd(), WM_CONTEXTMENU, reinterpret_cast<WPARAM>(first),
                       MAKELPARAM(10, 20)) != 125 || notifications.context_count != 1 ||
        notifications.forwarded_source != first)
        return 82;

    const DpiContext dpi = DpiContext::FromWindow(splitter.GetHwnd());
    RECT first_bounds = ChildBounds(splitter.GetHwnd(), first);
    RECT second_bounds = ChildBounds(splitter.GetHwnd(), second);
    if (!Near(first_bounds.right - first_bounds.left, dpi.ToPixels(Dip(120))) ||
        !Near(second_bounds.left, dpi.ToPixels(Dip(126))) ||
        !Near(second_bounds.right, dpi.ToPixels(Dip(400))))
        return 9;

    ::SendMessageW(splitter.GetHwnd(), WM_KEYDOWN, VK_RIGHT, 0);
    first_bounds = ChildBounds(splitter.GetHwnd(), first);
    if (!Near(first_bounds.right, dpi.ToPixels(Dip(128))) || notifications.count != 1 ||
        !Near(dpi.ToPixels(notifications.position), dpi.ToPixels(Dip(128))))
        return 10;

    ::SendMessageW(splitter.GetHwnd(), WM_KEYDOWN, VK_HOME, 0);
    first_bounds = ChildBounds(splitter.GetHwnd(), first);
    if (!Near(first_bounds.right, dpi.ToPixels(Dip(100)))) return 11;
    ::SendMessageW(splitter.GetHwnd(), WM_KEYDOWN, VK_END, 0);
    first_bounds = ChildBounds(splitter.GetHwnd(), first);
    if (!Near(first_bounds.right, dpi.ToPixels(Dip(314)))) return 12;

    if (!splitter.SetBounds(RectDip(Dip(0), Dip(0), Dip(300), Dip(160)))) return 13;
    second_bounds = ChildBounds(splitter.GetHwnd(), second);
    if (!Near(second_bounds.right, dpi.ToPixels(Dip(300)))) return 14;

    const int bar_start = dpi.ToPixels(splitter.GetPosition());
    const int drag_offset = dpi.ToPixels(Dip(3));
    const int destination = dpi.ToPixels(Dip(150));
    ::ShowWindow(parent, SW_SHOWNOACTIVATE);
    ::UpdateWindow(parent);
    ::SendMessageW(splitter.GetHwnd(), WM_LBUTTONDOWN, MK_LBUTTON,
                   MAKELPARAM(bar_start + drag_offset, dpi.ToPixels(Dip(20))));
    ::SendMessageW(splitter.GetHwnd(), WM_MOUSEMOVE, MK_LBUTTON,
                   MAKELPARAM(destination, dpi.ToPixels(Dip(20))));
    ::SendMessageW(splitter.GetHwnd(), WM_LBUTTONUP, 0,
                   MAKELPARAM(destination, dpi.ToPixels(Dip(20))));
    ::ShowWindow(parent, SW_HIDE);
    first_bounds = ChildBounds(splitter.GetHwnd(), first);
    if (!Near(first_bounds.right, dpi.ToPixels(Dip(147)))) return 15;

    const HWND outsider = ::CreateWindowExW(0, L"STATIC", L"outsider", WS_CHILD, 0, 0, 1, 1, parent,
                                            nullptr, ::GetModuleHandleW(nullptr), nullptr);
    if (outsider == nullptr || splitter.AttachPanes(first, outsider) ||
        ::GetLastError() != ERROR_INVALID_PARAMETER)
        return 16;
    ::DestroyWindow(outsider);

    ::DestroyWindow(first);
    if (splitter.SetConstraints({Dip(20), Dip(20), Dip(4)}) ||
        ::GetLastError() != ERROR_INVALID_WINDOW_HANDLE || splitter.GetFirstPane() != nullptr ||
        splitter.GetSecondPane() != nullptr)
        return 17;

    Splitter horizontal;
    if (!horizontal.Create(parent, ControlId{280}, RectDip(Dip(0), Dip(0), Dip(180), Dip(240)),
                           {.orientation = SplitterOrientation::horizontal,
                            .constraints = {Dip(60), Dip(70), Dip(4)},
                            .initial_position = Dip(90)}) ||
        (::GetWindowLongPtrW(horizontal.GetHwnd(), GWL_STYLE) & TBS_VERT) == 0)
        return 18;
    const HWND top =
        ::CreateWindowExW(0, L"STATIC", L"top", WS_CHILD, 0, 0, 1, 1, horizontal.GetHwnd(), nullptr,
                          ::GetModuleHandleW(nullptr), nullptr);
    const HWND bottom =
        ::CreateWindowExW(0, L"STATIC", L"bottom", WS_CHILD, 0, 0, 1, 1, horizontal.GetHwnd(),
                          nullptr, ::GetModuleHandleW(nullptr), nullptr);
    if (top == nullptr || bottom == nullptr || !horizontal.AttachPanes(top, bottom)) return 19;
    const RECT top_bounds = ChildBounds(horizontal.GetHwnd(), top);
    const RECT bottom_bounds = ChildBounds(horizontal.GetHwnd(), bottom);
    if (!Near(top_bounds.bottom, dpi.ToPixels(Dip(90))) ||
        !Near(bottom_bounds.top, dpi.ToPixels(Dip(94))) ||
        !Near(bottom_bounds.bottom, dpi.ToPixels(Dip(240))))
        return 20;
    horizontal.Destroy();

    const DWORD before = GuiResources();
    for (int iteration = 0; iteration < 100; ++iteration) {
        Splitter fixture;
        if (!fixture.Create(parent, ControlId{300 + iteration},
                            RectDip(Dip(0), Dip(0), Dip(200), Dip(100))))
            return 21;
        const HWND left =
            ::CreateWindowExW(0, L"STATIC", L"left", WS_CHILD, 0, 0, 1, 1, fixture.GetHwnd(),
                              nullptr, ::GetModuleHandleW(nullptr), nullptr);
        const HWND right =
            ::CreateWindowExW(0, L"STATIC", L"right", WS_CHILD, 0, 0, 1, 1, fixture.GetHwnd(),
                              nullptr, ::GetModuleHandleW(nullptr), nullptr);
        if (left == nullptr || right == nullptr || !fixture.AttachPanes(left, right)) {
            return 22;
        }
    }
    const DWORD after = GuiResources();
    if (after > before + 4) return 23;

    return EXIT_SUCCESS;
}
