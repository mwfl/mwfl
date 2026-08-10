#include <mwtl/mdi.h>

#include <windows.h>

#include <stdexcept>

namespace {

LRESULT CALLBACK FrameProcedure(HWND window, UINT message, WPARAM wparam,
                                LPARAM lparam) {
    return ::DefWindowProcW(window, message, wparam, lparam);
}

HWND CreateFrame() {
    constexpr wchar_t ClassName[] = L"mwtl.mdi.native.frame";
    WNDCLASSW window_class{};
    window_class.lpfnWndProc = FrameProcedure;
    window_class.hInstance = ::GetModuleHandleW(nullptr);
    window_class.lpszClassName = ClassName;
    if (!::RegisterClassW(&window_class) && ::GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        return nullptr;
    return ::CreateWindowExW(0, ClassName, L"MDI native test", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600, nullptr, nullptr,
        window_class.hInstance, nullptr);
}

}  // namespace

int main() {
    using namespace mwtl;
    MdiWorkspaceModel invalid_model;
    MdiHost invalid;
    if (invalid.Create(nullptr, invalid_model).status !=
        MdiHostStatus::invalid_argument) return 1;

    const DWORD initial_user = ::GetGuiResources(::GetCurrentProcess(), GR_USEROBJECTS);
    for (std::uint64_t cycle = 1; cycle <= 50; ++cycle) {
        HWND frame = CreateFrame();
        if (!frame) return 2;
        MdiWorkspaceModel model;
        if (!model.Add({{cycle * 10 + 1}, L"One"}) ||
            !model.Add({{cycle * 10 + 2}, L"Two", true, false})) return 3;
        MdiHost host;
        if (!host.Create(frame, model) || !host.GetClient()) return 4;
        host.Resize({0, 0, 800, 600});

        int callback_count = 0;
        auto first = host.CreateChild({cycle * 10 + 1}, {
            WS_VISIBLE | WS_OVERLAPPEDWINDOW, 77,
            [&](HWND, UINT message, WPARAM, LPARAM) -> std::optional<LRESULT> {
                if (message == WM_APP + 1) { ++callback_count; return 123; }
                if (message == WM_APP + 2) throw std::runtime_error("mdi child");
                return std::nullopt;
            }});
        auto second = host.CreateChild({cycle * 10 + 2});
        if (!first || !second || !::IsWindow(first.window) || !::IsWindow(second.window) ||
            host.GetChildId(first.window) != MdiChildId{cycle * 10 + 1}) return 5;
        if (::SendMessageW(first.window, WM_APP + 1, 0, 0) != 123 ||
            callback_count != 1) return 6;
        ::SendMessageW(first.window, WM_APP + 2, 0, 0);
        if (!host.TakeCallbackException() || host.TakeCallbackException()) return 7;
        if (!host.Activate({cycle * 10 + 1}) ||
            model.GetActiveId() != MdiChildId{cycle * 10 + 1}) return 8;
        if (!host.Arrange(MdiArrange::cascade) ||
            !host.Arrange(MdiArrange::tile_horizontal) ||
            !host.Arrange(MdiArrange::tile_vertical) ||
            !host.Arrange(MdiArrange::arrange_icons)) return 9;
        if (host.Close({cycle * 10 + 2}).status != MdiHostStatus::model_failure ||
            !host.Close({cycle * 10 + 2}, true) || model.Find({cycle * 10 + 2}) ||
            ::IsWindow(second.window)) return 10;
        if (!host.Close({cycle * 10 + 1}) || !model.GetChildren().empty()) return 11;
        host.Destroy();
        host.Destroy();
        if (host.GetClient()) return 12;
        ::DestroyWindow(frame);
    }
    const DWORD final_user = ::GetGuiResources(::GetCurrentProcess(), GR_USEROBJECTS);
    return final_user <= initial_user + 4 ? 0 : 13;
}
