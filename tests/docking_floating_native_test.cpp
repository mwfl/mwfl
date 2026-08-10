#include <mwtl/docking_floating.h>

#include <stdexcept>

int main() {
    using namespace mwtl;
    const HWND owner = ::CreateWindowExW(0, L"STATIC", L"floating owner",
        WS_OVERLAPPEDWINDOW, 0, 0, 600, 400, nullptr, nullptr,
        ::GetModuleHandleW(nullptr), nullptr);
    if (!owner) return 1;
    ::ShowWindow(owner, SW_SHOWNOACTIVATE);
    const HWND content = ::CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"tool",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 200, 120, owner,
        reinterpret_cast<HMENU>(101), ::GetModuleHandleW(nullptr), nullptr);
    if (!content) return 2;

    DockFloatingCloseAction action = DockFloatingCloseAction::cancel;
    int close_count = 0;
    DockFloatingWindow floating;
    DockFloatingWindowOptions options;
    options.title = L"Output tool";
    options.placement = {40, 50, 400, 300, L"primary"};
    options.target_dpi = 96;
    options.minimum_width = 200;
    options.minimum_height = 120;
    options.on_close = [&] {
        ++close_count;
        return action;
    };
    if (!floating.Create(owner, options) || floating.Create(owner).status !=
            DockFloatingWindowStatus::invalid_argument ||
        !floating.AttachContent(content) || floating.AttachContent(content).status !=
            DockFloatingWindowStatus::already_attached ||
        ::GetParent(content) != floating.GetHwnd()) return 3;
    floating.Show(false);
    if (!floating.IsVisible() || ::IsWindowVisible(content) == FALSE) return 4;
    ::SetWindowPos(floating.GetHwnd(), nullptr, 40, 50, 500, 360,
                   SWP_NOZORDER | SWP_NOACTIVATE);
    RECT client{}, content_bounds{};
    ::GetClientRect(floating.GetHwnd(), &client);
    ::GetWindowRect(content, &content_bounds);
    ::MapWindowPoints(nullptr, floating.GetHwnd(),
                      reinterpret_cast<POINT*>(&content_bounds), 2);
    if (content_bounds.left != 0 || content_bounds.top != 0 ||
        content_bounds.right != client.right || content_bounds.bottom != client.bottom)
        return 5;
    MINMAXINFO minimum{};
    ::SendMessageW(floating.GetHwnd(), WM_GETMINMAXINFO, 0,
                   reinterpret_cast<LPARAM>(&minimum));
    if (minimum.ptMinTrackSize.x < 200 || minimum.ptMinTrackSize.y < 120) return 6;

    if (!floating.SetPlacement({10, 20, 300, 200, L"primary"}, 144)) return 7;
    RECT placed{};
    ::GetWindowRect(floating.GetHwnd(), &placed);
    if (placed.left != 15 || placed.top != 30 || placed.right - placed.left != 450 ||
        placed.bottom - placed.top != 300) return 8;
    RECT suggested{100, 110, 510, 420};
    ::SendMessageW(floating.GetHwnd(), WM_DPICHANGED, MAKELONG(192, 192),
                   reinterpret_cast<LPARAM>(&suggested));
    ::GetWindowRect(floating.GetHwnd(), &placed);
    if (placed.left != 100 || placed.top != 110 || placed.right != 510 ||
        placed.bottom != 420) return 9;

    ::SendMessageW(floating.GetHwnd(), WM_CLOSE, 0, 0);
    if (!floating.IsVisible() || close_count != 1) return 10;
    action = DockFloatingCloseAction::hide;
    ::SendMessageW(floating.GetHwnd(), WM_CLOSE, 0, 0);
    if (floating.IsVisible() || close_count != 2 || floating.GetContent() != content)
        return 11;
    floating.Show(false);
    action = DockFloatingCloseAction::destroy;
    ::SendMessageW(floating.GetHwnd(), WM_KEYDOWN, VK_ESCAPE, 0);
    if (floating.GetHwnd() || floating.GetContent() || ::GetParent(content) != owner ||
        close_count != 3 || ::IsWindow(content) == FALSE) return 12;
    MSG message{};
    if (::PeekMessageW(&message, nullptr, WM_QUIT, WM_QUIT, PM_REMOVE)) return 13;

    DockFloatingWindow throwing;
    DockFloatingWindowOptions throwing_options;
    throwing_options.placement = {20, 20, 300, 200, L"primary"};
    throwing_options.on_close = []() -> DockFloatingCloseAction {
        throw std::runtime_error("close callback");
    };
    if (!throwing.Create(owner, throwing_options) || !throwing.AttachContent(content))
        return 14;
    throwing.Show(false);
    ::SendMessageW(throwing.GetHwnd(), WM_CLOSE, 0, 0);
    if (!throwing.IsVisible() || !throwing.GetLastCallbackError() ||
        ::GetParent(content) != throwing.GetHwnd()) return 15;
    if (!throwing.DetachContent() || ::GetParent(content) != owner ||
        throwing.DetachContent().status != DockFloatingWindowStatus::not_attached)
        return 16;
    throwing.Destroy();
    throwing.Destroy();
    if (!::IsWindow(content)) return 17;
    ::DestroyWindow(owner);
    return 0;
}
