#include <mwtl/ribbon.h>

#include <windows.h>

#include <cstdint>

namespace {

LRESULT CALLBACK TestWindowProcedure(HWND window, UINT message, WPARAM wparam,
                                     LPARAM lparam) {
    return ::DefWindowProcW(window, message, wparam, lparam);
}

HWND CreateTestWindow() {
    constexpr wchar_t ClassName[] = L"mwtl.ribbon.native.test";
    WNDCLASSW window_class{};
    window_class.lpfnWndProc = TestWindowProcedure;
    window_class.hInstance = ::GetModuleHandleW(nullptr);
    window_class.lpszClassName = ClassName;
    if (!::RegisterClassW(&window_class) && ::GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        return nullptr;
    return ::CreateWindowExW(0, ClassName, L"Ribbon native test", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 640, 480, nullptr, nullptr,
        window_class.hInstance, nullptr);
}

}  // namespace

int main() {
    using namespace mwtl;
    RibbonCommandModel model;
    CommandSet commands;
    int invoked = 0;
    commands.Add(Command({1}, L"Invoke", [&] { ++invoked; }));
    if (!model.Add({{1010}, {1}, 1})) return 1;

    RibbonFrameworkHost invalid;
    if (invalid.Create(nullptr, model, commands).status !=
            RibbonHostStatus::invalid_argument ||
        invalid.Load(::GetModuleHandleW(nullptr), L"TEST_RIBBON_RIBBON").status !=
            RibbonHostStatus::invalid_state)
        return 2;

    const HRESULT initialized = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(initialized)) return 3;
    HWND window = CreateTestWindow();
    if (!window) {
        ::CoUninitialize();
        return 4;
    }

    for (int cycle = 0; cycle != 25; ++cycle) {
        RibbonFrameworkHost host;
        const auto created = host.Create(window, model, commands);
        if (!created) {
            ::DestroyWindow(window);
            ::CoUninitialize();
            return created.status == RibbonHostStatus::unavailable ? 0 : 5;
        }
        if (!host.IsCreated() || host.GetOwner() != window || !host.GetFramework())
            return 6;
        if (!host.Load(::GetModuleHandleW(nullptr), L"TEST_RIBBON_RIBBON")) return 7;
        if (!host.SetModes(1) ||
            host.SetModes(0).status != RibbonHostStatus::invalid_argument)
            return 8;
        if (!host.Invalidate({1010}) || !host.InvalidateAll() ||
            host.Invalidate({9999}).status != RibbonHostStatus::invalid_argument)
            return 9;
        host.Destroy();
        host.Destroy();
        if (host.IsCreated() || host.GetOwner() || host.GetFramework() ||
            host.GetHeight() != 0)
            return 10;
    }

    ::DestroyWindow(window);
    ::CoUninitialize();

    // The host rejects an initialized multithreaded apartment before creating
    // Ribbon COM objects.
    const HRESULT mta = ::CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (SUCCEEDED(mta)) {
        window = CreateTestWindow();
        RibbonFrameworkHost wrong_apartment;
        const auto result = wrong_apartment.Create(window, model, commands);
        ::DestroyWindow(window);
        ::CoUninitialize();
        if (result.status != RibbonHostStatus::wrong_apartment) return 11;
    }
    return invoked == 0 ? 0 : 12;
}
