#include <mwfl/docking_preview.h>

#include <cstdlib>
#include <cstdio>
#include <string_view>

namespace {

DWORD Resources(DWORD kind) {
    return ::GetGuiResources(::GetCurrentProcess(), kind);
}

HWND MakeOwner() {
    return ::CreateWindowExW(0, L"STATIC", L"Dock preview test owner",
        WS_OVERLAPPEDWINDOW, 0, 0, 320, 240, nullptr, nullptr,
        ::GetModuleHandleW(nullptr), nullptr);
}

}  // namespace

int main() {
    using namespace mwfl;
    HWND owner = MakeOwner();
    if (!owner) return 1;
    ::ShowWindow(owner, SW_SHOWNOACTIVATE);
    HWND focus = ::CreateWindowExW(0, L"EDIT", L"focus", WS_CHILD | WS_VISIBLE,
        0, 0, 40, 20, owner, nullptr, ::GetModuleHandleW(nullptr), nullptr);
    if (!focus) return 2;
    ::SetFocus(focus);

    DockPreviewWindow preview;
    if (!preview.Create(owner, {.opacity = 120, .border_pixels = 4}) ||
        preview.Create(owner)) return 3;
    const HWND preview_hwnd = preview.GetHwnd();
    const LONG_PTR ex_style = ::GetWindowLongPtrW(preview_hwnd, GWL_EXSTYLE);
    if ((ex_style & (WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT |
                     WS_EX_LAYERED)) !=
        (WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT | WS_EX_LAYERED))
        return 4;
    const HWND focus_before_show = ::GetFocus();
    const bool shown = preview.Show({10, 20, 100, 80}, 144);
    if (!shown || !preview.IsVisible() || ::GetFocus() != focus_before_show) {
        std::fprintf(stderr, "preview show=%d visible=%d focus=%p expected=%p error=%lu\n",
            shown, preview.IsVisible(), static_cast<void*>(::GetFocus()),
            static_cast<void*>(focus_before_show), static_cast<unsigned long>(::GetLastError()));
        return 5;
    }
    RECT bounds{};
    if (!::GetWindowRect(preview_hwnd, &bounds) || bounds.left != 15 ||
        bounds.top != 30 || bounds.right - bounds.left != 150 ||
        bounds.bottom - bounds.top != 120) return 6;
    if (::SendMessageW(preview_hwnd, WM_NCHITTEST, 0, 0) != HTTRANSPARENT ||
        ::SendMessageW(preview_hwnd, WM_MOUSEACTIVATE, 0, 0) != MA_NOACTIVATE)
        return 7;
    wchar_t title[64]{};
    if (::GetWindowTextW(preview_hwnd, title, 64) <= 0 ||
        std::wstring_view{title} != L"Docking preview") return 8;
    ::UpdateWindow(preview_hwnd);
    preview.Hide();
    if (preview.IsVisible() || preview.Show({0, 0, -1, 20}, 96) ||
        preview.Show({0, 0, 20, 20}, 0)) return 9;
    preview.Destroy();
    preview.Destroy();
    if (preview.GetHwnd() || ::IsWindow(preview_hwnd)) return 10;

    // Warm the registered class and drawing path before measuring process deltas.
    {
        DockPreviewWindow warm;
        if (!warm.Create(owner) || !warm.Show({0, 0, 40, 40}, 96)) return 11;
        ::UpdateWindow(warm.GetHwnd());
    }
    const DWORD gdi_before = Resources(GR_GDIOBJECTS);
    const DWORD user_before = Resources(GR_USEROBJECTS);
    for (int i = 0; i < 100; ++i) {
        DockPreviewWindow current;
        if (!current.Create(owner) || !current.Show({1, 1, 30, 30}, 96)) return 12;
        ::UpdateWindow(current.GetHwnd());
        current.Hide();
    }
    const DWORD gdi_after = Resources(GR_GDIOBJECTS);
    const DWORD user_after = Resources(GR_USEROBJECTS);
    if (gdi_after > gdi_before + 2 || user_after > user_before + 2) return 13;

    DockPreviewWindow owner_teardown;
    if (!owner_teardown.Create(owner) || !owner_teardown.Show({0, 0, 20, 20}, 96))
        return 14;
    if (!::DestroyWindow(owner) || owner_teardown.GetHwnd() != nullptr) return 15;
    owner_teardown.Destroy();
    return 0;
}
