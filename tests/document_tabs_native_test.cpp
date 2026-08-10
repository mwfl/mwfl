#include <mwfl/document_tabs.h>

#include <commctrl.h>

#include <cstdio>

namespace {

HWND MakeHost(int x) {
    return ::CreateWindowExW(0, L"STATIC", L"host", WS_OVERLAPPEDWINDOW,
                             x, 50, 500, 350, nullptr, nullptr,
                             ::GetModuleHandleW(nullptr), nullptr);
}

HWND MakePage(HWND parent, int id) {
    return ::CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"page",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_MULTILINE, 0, 0, 10, 10,
        parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        ::GetModuleHandleW(nullptr), nullptr);
}

bool HasVisibleStyle(HWND window) {
    return (::GetWindowLongPtrW(window, GWL_STYLE) & WS_VISIBLE) != 0;
}

}  // namespace

int main() {
    using namespace mwfl;
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_TAB_CLASSES};
    if (!::InitCommonControlsEx(&controls)) return 1;
    // Warm process-wide Common Controls brushes/fonts before measuring the
    // per-workspace lifecycle delta.
    const HWND warm_host = MakeHost(0);
    const HWND warm_tab = ::CreateWindowExW(0, WC_TABCONTROLW, L"", WS_CHILD,
        0, 0, 10, 10, warm_host, nullptr, ::GetModuleHandleW(nullptr), nullptr);
    const HWND warm_edit = MakePage(warm_host, 1);
    TCITEMW warm_item{};
    wchar_t warm_title[] = L"warm";
    warm_item.mask = TCIF_TEXT;
    warm_item.pszText = warm_title;
    RECT warm_bounds{0, 0, 400, 250};
    TabCtrl_InsertItem(warm_tab, 0, &warm_item);
    TabCtrl_AdjustRect(warm_tab, FALSE, &warm_bounds);
    ::ShowWindow(warm_host, SW_SHOWNOACTIVATE);
    ::UpdateWindow(warm_host);
    ::SetFocus(warm_edit);
    if (!warm_host || !warm_tab || !warm_edit || !::DestroyWindow(warm_host)) return 26;
    const DWORD gdi_before = ::GetGuiResources(::GetCurrentProcess(), GR_GDIOBJECTS);
    const DWORD user_before = ::GetGuiResources(::GetCurrentProcess(), GR_USEROBJECTS);
    const HWND left_host = MakeHost(50);
    const HWND right_host = MakeHost(600);
    if (!left_host || !right_host) return 2;

    TabControl left_tabs;
    TabControl right_tabs;
    const RectDip tab_bounds{Dip{0}, Dip{0}, Dip{480}, Dip{300}};
    if (!left_tabs.Create(left_host, {101}, tab_bounds) ||
        !right_tabs.Create(right_host, {102}, tab_bounds)) return 3;
    const HWND first_page = MakePage(left_host, 201);
    const HWND second_page = MakePage(left_host, 202);
    const HWND wrong_parent = MakePage(right_host, 203);
    if (!first_page || !second_page || !wrong_parent) return 4;

    DocumentWorkspaceModel left{{1}};
    DocumentWorkspaceModel right{{2}};
    if (!left.Add({{11}, L"First", {}, true}) ||
        !left.Add({{22}, L"Second"})) return 5;
    DocumentTabWorkspaceAdapter left_adapter;
    DocumentTabWorkspaceAdapter right_adapter;
    if (left_adapter.Attach(left_tabs) != DocumentTabStatus::success ||
        right_adapter.Attach(right_tabs) != DocumentTabStatus::success ||
        left_adapter.BindPage({11}, first_page) != DocumentTabStatus::success ||
        left_adapter.BindPage({22}, second_page) != DocumentTabStatus::success ||
        left_adapter.BindPage({33}, wrong_parent) != DocumentTabStatus::invalid_parent)
        return 6;
    if (!left_adapter.Synchronize(left) || TabCtrl_GetItemCount(left_tabs.GetHwnd()) != 2 ||
        !HasVisibleStyle(first_page) || HasVisibleStyle(second_page)) return 7;
    TCITEMW native{};
    native.mask = TCIF_PARAM;
    if (TabCtrl_GetItem(left_tabs.GetHwnd(), 0, &native) == FALSE ||
        native.lParam != 11) return 8;

    ::SetFocus(left_tabs.GetHwnd());
    ::SendMessageW(left_tabs.GetHwnd(), WM_KEYDOWN, VK_RIGHT, 0);
    if (!left_adapter.ActivateNativeSelection(left) ||
        left.GetActiveId() != DocumentId{22} || !HasVisibleStyle(second_page) ||
        ::GetFocus() != second_page) return 9;
    ::SetWindowPos(left_tabs.GetHwnd(), nullptr, 0, 0, 420, 260,
                   SWP_NOACTIVATE | SWP_NOZORDER);
    if (left_adapter.ArrangePages() != DocumentTabStatus::success) return 10;
    RECT page_bounds{};
    if (!::GetWindowRect(second_page, &page_bounds) ||
        page_bounds.right <= page_bounds.left || page_bounds.bottom <= page_bounds.top)
        return 11;

    if (!left.Rename({22}, L"Second", L"C:\\Docs\\same.txt") ||
        !right.Add({{99}, L"Conflict", L"c:/docs/SAME.txt"})) return 22;
    const auto rejected_transfer = TransferDocumentWithPage(
        left, left_adapter, right, right_adapter, {22});
    if (rejected_transfer.status != DocumentTabStatus::model_failure ||
        !left.Find({22}) || right.Find({22}) || ::GetParent(second_page) !=
            left_host) return 23;
    if (!right.Close({99}, false) || !left.Rename({22}, L"Second")) return 24;

    const auto transferred = TransferDocumentWithPage(
        left, left_adapter, right, right_adapter, {22});
    if (!transferred || left.Find({22}) || !right.Find({22}) ||
        left_adapter.FindPage({22}) || right_adapter.FindPage({22}) != second_page ||
        ::GetParent(second_page) != right_host ||
        right.GetActiveId() != DocumentId{22} || ::GetFocus() != second_page) return 12;

    if (!left.Move({11}, 0) || !left_adapter.Synchronize(left)) return 13;
    if (!left.Close({11}, false) ||
        left_adapter.UnbindPage({11}) != DocumentTabStatus::success) return 14;
    ::DestroyWindow(first_page);
    if (!left_adapter.Synchronize(left)) return 15;

    ::DestroyWindow(second_page);
    if (right_adapter.Synchronize(right).status != DocumentTabStatus::stale_window)
        return 16;
    right_adapter.Detach();
    left_adapter.Detach();
    ::DestroyWindow(wrong_parent);
    ::DestroyWindow(left_host);
    ::DestroyWindow(right_host);
    const DWORD gdi_after = ::GetGuiResources(::GetCurrentProcess(), GR_GDIOBJECTS);
    const DWORD user_after = ::GetGuiResources(::GetCurrentProcess(), GR_USEROBJECTS);
    // The first fully synchronized themed tab workspace leaves up to three
    // process-wide Common Controls GDI cache entries. Repeated workspaces do
    // not grow beyond this fixed cache; USER objects must still stay within
    // the normal two-object measurement jitter.
    if (gdi_after > gdi_before + 3 || user_after > user_before + 2) {
        std::fprintf(stderr, "resource delta: gdi=%lu user=%lu\n",
            static_cast<unsigned long>(gdi_after - gdi_before),
            static_cast<unsigned long>(user_after - user_before));
        return 25;
    }
    return 0;
}
