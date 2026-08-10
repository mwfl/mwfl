#include <mwfl/window_options.h>

#include <wil/resource.h>

#include <algorithm>
#include <cstring>

namespace mwfl::detail {
namespace {

UINT GetMonitorDpi(HMONITOR monitor) noexcept {
    using GetDpiForMonitorFn = HRESULT(WINAPI*)(HMONITOR, int, UINT*, UINT*);
    wil::unique_hmodule module(::LoadLibraryExW(
        L"shcore.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32));
    if (module) {
        const FARPROC address =
            ::GetProcAddress(module.get(), "GetDpiForMonitor");
        static_assert(sizeof(address) == sizeof(GetDpiForMonitorFn));
        GetDpiForMonitorFn get_dpi = nullptr;
        std::memcpy(&get_dpi, &address, sizeof(get_dpi));
        UINT x = DpiContext::kDefaultDpi;
        UINT y = DpiContext::kDefaultDpi;
        // MDT_EFFECTIVE_DPI is zero. Avoid a Shcore header/link dependency and
        // keep the Windows 10 1809 startup surface dynamic.
        if (get_dpi != nullptr && SUCCEEDED(get_dpi(monitor, 0, &x, &y)) && x != 0) {
            return x;
        }
    }
    const UINT system_dpi = ::GetDpiForSystem();
    return system_dpi == 0 ? DpiContext::kDefaultDpi : system_dpi;
}

}  // namespace

RECT ResolveWindowBounds(const WindowOptions& options) noexcept {
    const POINT requested_origin =
        DpiContext{}.ToPixels(options.initial_bounds.origin);
    POINT monitor_point = requested_origin;
    if (options.center_in_work_area) {
        monitor_point = POINT{0, 0};
    }

    const HMONITOR monitor = ::MonitorFromPoint(
        monitor_point,
        options.center_in_work_area ? MONITOR_DEFAULTTOPRIMARY
                                    : MONITOR_DEFAULTTONEAREST);
    MONITORINFO monitor_info{};
    monitor_info.cbSize = sizeof(monitor_info);
    if (::GetMonitorInfoW(monitor, &monitor_info) == FALSE) {
        monitor_info.rcWork = RECT{0, 0, 1280, 800};
    }

    const DpiContext dpi = DpiContext::FromDpi(GetMonitorDpi(monitor));
    const SIZE requested_size = dpi.ToPixels(options.initial_bounds.size);
    RECT outer{0, 0, (std::max)(1L, requested_size.cx),
               (std::max)(1L, requested_size.cy)};
    if (options.bounds_are_client_size) {
        ::AdjustWindowRectExForDpi(
            &outer,
            options.style,
            FALSE,
            options.extended_style,
            dpi.GetDpi());
    }

    int width = outer.right - outer.left;
    int height = outer.bottom - outer.top;
    const int work_width = monitor_info.rcWork.right - monitor_info.rcWork.left;
    const int work_height = monitor_info.rcWork.bottom - monitor_info.rcWork.top;
    width = (std::min)((std::max)(1, width), (std::max)(1, work_width));
    height = (std::min)((std::max)(1, height), (std::max)(1, work_height));

    int x = dpi.ToPixels(options.initial_bounds.origin.x);
    int y = dpi.ToPixels(options.initial_bounds.origin.y);
    if (options.center_in_work_area) {
        x = monitor_info.rcWork.left + (work_width - width) / 2;
        y = monitor_info.rcWork.top + (work_height - height) / 2;
    } else {
        const int work_left = static_cast<int>(monitor_info.rcWork.left);
        const int work_top = static_cast<int>(monitor_info.rcWork.top);
        const int maximum_x = static_cast<int>(monitor_info.rcWork.right) - width;
        const int maximum_y = static_cast<int>(monitor_info.rcWork.bottom) - height;
        x = (std::min)((std::max)(x, work_left), maximum_x);
        y = (std::min)((std::max)(y, work_top), maximum_y);
    }
    return RECT{x, y, x + width, y + height};
}

}  // namespace mwfl::detail
