#include <mwfl/layout.h>
#include <mwfl/controls.h>

#include <windows.h>
#include <wil/resource.h>

#include <cstdlib>
#include <atomic>
#include <stdexcept>
#include <thread>

namespace {

RECT ChildBounds(HWND parent, HWND child) {
    RECT bounds{};
    ::GetWindowRect(child, &bounds);
    ::MapWindowPoints(nullptr, parent, reinterpret_cast<POINT*>(&bounds), 2);
    return bounds;
}

bool Near(int actual, int expected, int tolerance = 1) {
    return actual >= expected - tolerance && actual <= expected + tolerance;
}

}  // namespace

int main() {
    using namespace mwfl;
    using mwfl::operator""_dip;

    const HWND parent = ::CreateWindowExW(
        0, L"STATIC", L"layout parent", WS_OVERLAPPED,
        0, 0, 420, 260, nullptr, nullptr, ::GetModuleHandleW(nullptr), nullptr);
    if (parent == nullptr) return 1;
    const auto destroy_parent = wil::scope_exit([&] { ::DestroyWindow(parent); });

    const auto child = [&](const wchar_t* name) {
        return ::CreateWindowExW(
            0, L"STATIC", name, WS_CHILD,
            0, 0, 10, 10, parent, nullptr, ::GetModuleHandleW(nullptr), nullptr);
    };
    const HWND top = child(L"top");
    const HWND left = child(L"left");
    const HWND right = child(L"right");
    const HWND bottom = child(L"bottom");
    const HWND overlay_back = child(L"overlay back");
    const HWND overlay_front = child(L"overlay front");
    const HWND shrink_auto = child(L"shrink auto");
    const HWND shrink_fixed = child(L"shrink fixed");
    const HWND capped = child(L"capped stretch");
    const HWND native_container = child(L"native container");
    const HWND nested_child = ::CreateWindowExW(
        0, L"STATIC", L"nested child", WS_CHILD,
        0, 0, 10, 10, native_container, nullptr,
        ::GetModuleHandleW(nullptr), nullptr);
    if (!top || !left || !right || !bottom || !overlay_back || !overlay_front ||
        !shrink_auto || !shrink_fixed || !capped || !native_container ||
        !nested_child) return 2;

    auto middle = Row();
    middle.Gap(5_dip)
        .Add(left, Fixed(100_dip))
        .Add(right, Stretch(1.0f, 50_dip));

    auto root = Column();
    auto overlay = Overlay();
    overlay.Add(overlay_back).Add(overlay_front);
    root.Margin(10_dip).Gap(10_dip)
        .Add(top, Fixed(30_dip))
        .Add(std::move(middle), Stretch(1.0f, 60_dip))
        .Add(std::move(overlay), Fixed(24_dip))
        .Add(bottom, Auto(), {
            .alignment = CrossAlignment::center,
            .preferred_size = SizeDip{100_dip, 20_dip},
        });

    LayoutHost layout(std::move(root));
    if (!layout.Arrange(parent)) return 3;

    RECT client{};
    ::GetClientRect(parent, &client);
    const DpiContext dpi = DpiContext::FromWindow(parent);
    const int margin = dpi.ToPixels(10_dip);
    const int fixed_top = dpi.ToPixels(30_dip);
    const int fixed_left = dpi.ToPixels(100_dip);
    const int gap = dpi.ToPixels(5_dip);
    const RECT top_bounds = ChildBounds(parent, top);
    const RECT left_bounds = ChildBounds(parent, left);
    const RECT right_before = ChildBounds(parent, right);
    const RECT bottom_bounds = ChildBounds(parent, bottom);
    const RECT overlay_back_bounds = ChildBounds(parent, overlay_back);
    const RECT overlay_front_bounds = ChildBounds(parent, overlay_front);
    if (!Near(top_bounds.left, margin) || !Near(top_bounds.top, margin) ||
        !Near(top_bounds.bottom - top_bounds.top, fixed_top) ||
        !Near(top_bounds.right, client.right - margin)) {
        return 4;
    }
    if (!Near(left_bounds.left, margin) ||
        !Near(left_bounds.right - left_bounds.left, fixed_left) ||
        !Near(right_before.left, left_bounds.right + gap)) {
        return 5;
    }
    if (!Near(bottom_bounds.right - bottom_bounds.left, dpi.ToPixels(100_dip)) ||
        !Near(bottom_bounds.left,
              (client.right - dpi.ToPixels(100_dip)) / 2)) {
        return 6;
    }
    if (overlay_back_bounds.left != overlay_front_bounds.left ||
        overlay_back_bounds.top != overlay_front_bounds.top ||
        overlay_back_bounds.right != overlay_front_bounds.right ||
        overlay_back_bounds.bottom != overlay_front_bounds.bottom) {
        return 9;
    }

    if (::SetWindowPos(parent, nullptr, 0, 0, 620, 360,
                       SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOZORDER) == FALSE ||
        !layout.Arrange(parent)) {
        return 7;
    }
    const RECT left_after = ChildBounds(parent, left);
    const RECT right_after = ChildBounds(parent, right);
    if (!Near(left_after.right - left_after.left, fixed_left) ||
        right_after.right - right_after.left <=
            right_before.right - right_before.left) {
        return 8;
    }

    ::GetClientRect(parent, &client);
    const Dip client_width = dpi.FromPixels(client.right - client.left);
    auto shrink_root = Row();
    shrink_root
        .Add(shrink_auto, Auto(20_dip), {
            .preferred_size = SizeDip{100_dip, 20_dip},
        })
        .Add(shrink_fixed, Fixed(client_width - 30_dip));
    LayoutHost shrink_layout(std::move(shrink_root));
    if (!shrink_layout.Arrange(parent)) return 10;
    const RECT shrink_bounds = ChildBounds(parent, shrink_auto);
    if (!Near(shrink_bounds.right - shrink_bounds.left, dpi.ToPixels(30_dip)))
        return 11;

    auto capped_root = Row();
    capped_root.Add(capped, Stretch(1.0f, 20_dip, 80_dip));
    LayoutHost capped_layout(std::move(capped_root));
    if (!capped_layout.Arrange(parent)) return 12;
    const RECT capped_bounds = ChildBounds(parent, capped);
    if (!Near(capped_bounds.right - capped_bounds.left, dpi.ToPixels(80_dip)))
        return 13;

    if (LayoutHost{}.Arrange(parent) || layout.Arrange(nullptr)) return 14;

    auto native_overlay = Overlay();
    native_overlay.Add(native_container).Add(nested_child);
    LayoutHost native_layout(std::move(native_overlay));
    if (!native_layout.Arrange(parent)) return 15;
    const RECT nested_bounds = ChildBounds(native_container, nested_child);
    RECT native_client{};
    ::GetClientRect(native_container, &native_client);
    if (!Near(nested_bounds.left, 0) || !Near(nested_bounds.top, 0) ||
        !Near(nested_bounds.right, native_client.right) ||
        !Near(nested_bounds.bottom, native_client.bottom)) {
        return 16;
    }

    auto duplicate = Row();
    duplicate.Add(top).Add(top);
    LayoutHost duplicate_layout(std::move(duplicate));
    if (duplicate_layout.Arrange(parent)) return 17;

    bool null_rejected = false;
    try {
        auto invalid = Row();
        invalid.Add(nullptr);
    } catch (const std::invalid_argument&) {
        null_rejected = true;
    }
    if (!null_rejected) return 18;

    LayoutHost fluent_layout(
        Row()
            .Margin(4_dip)
            .Gap(3_dip)
            .Add(top, Fixed(20_dip))
            .Add(bottom, Stretch()));
    if (!fluent_layout.HasRoot() || !fluent_layout.Arrange(parent)) return 19;

    static_assert(IntrinsicallyMeasurable<Label>);
    Label measured_label;
    if (!measured_label.Create(parent, {501}, L"intrinsic label", {})) return 20;
    if (!measured_label.IsOwnerThread()) return 28;
    std::atomic<bool> worker_rejected{false};
    std::thread ownership_probe([&] {
        worker_rejected.store(
            !measured_label.IsOwnerThread(), std::memory_order_release);
    });
    ownership_probe.join();
    if (!worker_rejected.load(std::memory_order_acquire)) return 29;
    Label move_source;
    if (!move_source.Create(parent, {502}, L"move ownership", {})) return 30;
    LayoutHost moved_layout(Column().Add(move_source, Auto()));
    Label move_target(std::move(move_source));
    if (!move_target.IsOwnerThread() || move_source.IsWindow() ||
        !move_target.IsWindow()) {
        return 31;
    }
    std::thread moved_ownership_probe([&] {
        worker_rejected.store(
            !move_target.IsOwnerThread(), std::memory_order_release);
    });
    moved_ownership_probe.join();
    if (!worker_rejected.load(std::memory_order_acquire)) return 32;
    const SizeDip moved_preferred = moved_layout.GetPreferredSize(parent);
    if (moved_preferred.width.value <= 0.0f ||
        moved_preferred.height.value <= 0.0f) {
        return 33;
    }
    const SizeDip intrinsic = measured_label.GetPreferredSize(dpi);
    if (intrinsic.width.value <= 0.0f || intrinsic.height.value <= 0.0f)
        return 21;
    LayoutHost intrinsic_layout(Column().Add(measured_label, Auto()));
    const SizeDip preferred = intrinsic_layout.GetPreferredSize(parent);
    if (preferred.width.value <= 0.0f || preferred.height.value <= 0.0f)
        return 22;
    if (!measured_label.SetText(
            L"intrinsic label whose text changed after joining layout"))
        return 23;
    const SizeDip updated_preferred = intrinsic_layout.GetPreferredSize(parent);
    if (updated_preferred.width.value <= preferred.width.value) return 24;

    const HFONT large_font = ::CreateFontW(
        -32, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH, L"Segoe UI");
    if (large_font == nullptr) return 26;
    const auto delete_large_font = wil::scope_exit([&] {
        ::SendMessageW(
            measured_label.GetHwnd(), WM_SETFONT,
            reinterpret_cast<WPARAM>(::GetStockObject(DEFAULT_GUI_FONT)), FALSE);
        ::DeleteObject(large_font);
    });
    ::SendMessageW(
        measured_label.GetHwnd(), WM_SETFONT,
        reinterpret_cast<WPARAM>(large_font), FALSE);
    const SizeDip font_preferred = intrinsic_layout.GetPreferredSize(parent);
    if (font_preferred.height.value <= updated_preferred.height.value) return 27;

    LayoutHost dip_length_layout(Row().Add(measured_label, 80_dip));
    if (!dip_length_layout.Arrange(parent)) return 25;
    return EXIT_SUCCESS;
}
