#include <mwtl/webview2.h>

#include <objbase.h>

#include <chrono>
#include <filesystem>

using mwtl::operator""_dip;

namespace {
bool PumpUntil(const bool& finished, std::chrono::seconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (!finished && std::chrono::steady_clock::now() < deadline) {
        MSG message{};
        while (::PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            ::TranslateMessage(&message);
            ::DispatchMessageW(&message);
        }
        ::MsgWaitForMultipleObjectsEx(0, nullptr, 20, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
    }
    return finished;
}
}  // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    const HRESULT apartment = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(apartment)) return 1;
    struct Apartment { ~Apartment() { ::CoUninitialize(); } } apartment_guard;

    const HWND parent = ::CreateWindowExW(0, L"STATIC", L"WebView2 native test",
        WS_OVERLAPPEDWINDOW, 0, 0, 640, 480, nullptr, nullptr, nullptr, nullptr);
    if (parent == nullptr) return 2;
    struct Window { HWND value; ~Window() { if (::IsWindow(value)) ::DestroyWindow(value); } } window{parent};

    mwtl::WebView2Host host;
    if (!host.Create(parent, {1200}, {0.0_dip, 0.0_dip, 500.0_dip, 300.0_dip})) return 3;
    if (host.GetState() != mwtl::WebView2HostState::created || host.IsReady()) return 4;

    const auto runtime = mwtl::QueryWebView2Runtime();
    bool initialized = false;
    bool restarted = false;
    bool navigated = false;
    int initialization_count = 0;
    mwtl::WebView2InitializationResult initialization{};
    const auto data = std::filesystem::temp_directory_path() /
        (L"mwtl-webview2-native-" + std::to_wstring(::GetCurrentProcessId()));
    if (!host.Initialize({.user_data_folder = data}, {
            .initialized = [&](mwtl::WebView2InitializationResult result) {
                initialization = result;
                initialized = true;
                ++initialization_count;
                restarted = initialization_count >= 2;
                if (result.state == mwtl::WebView2HostState::ready)
                    static_cast<void>(host.NavigateToString(
                        L"<!doctype html><html><body><button autofocus>offline</button></body></html>"));
            },
            .navigation_completed = [&](bool succeeded, HRESULT) { navigated = succeeded; }})) return 5;
    if (!PumpUntil(initialized, std::chrono::seconds{20})) return 6;

    if (runtime.status == mwtl::WebView2RuntimeStatus::missing) {
        if (initialization.state != mwtl::WebView2HostState::failed ||
            initialization.runtime != mwtl::WebView2RuntimeStatus::missing) return 7;
    } else if (runtime.status == mwtl::WebView2RuntimeStatus::available) {
        if (initialization.state != mwtl::WebView2HostState::ready || !host.IsReady() ||
            host.GetController() == nullptr || host.GetWebView() == nullptr) return 8;
        if (!PumpUntil(navigated, std::chrono::seconds{15})) return 9;
        if (!host.SetBounds({5.0_dip, 6.0_dip, 420.0_dip, 240.0_dip}) || !host.Arrange() ||
            !host.FocusContent() || !host.Reload() || !host.Stop()) return 10;
        navigated = false;
        if (!host.Restart() || !PumpUntil(restarted, std::chrono::seconds{20}) ||
            !PumpUntil(navigated, std::chrono::seconds{15}) || !host.IsReady()) return 11;
    } else {
        if (initialization.state != mwtl::WebView2HostState::failed) return 12;
    }

    host.Close();
    if (host.GetState() != mwtl::WebView2HostState::closed || host.IsReady() ||
        host.GetController() != nullptr || host.GetWebView() != nullptr ||
        host.Navigate(L"https://example.invalid")) return 13;
    host.Close();

    if (runtime.status == mwtl::WebView2RuntimeStatus::available) {
        mwtl::WebView2Host cancelled;
        bool late_callback = false;
        const auto cancelled_data = data.wstring() + L"-cancelled";
        if (!cancelled.Create(parent, {1201}, {0.0_dip, 0.0_dip, 200.0_dip, 120.0_dip}) ||
            !cancelled.Initialize({.user_data_folder = cancelled_data}, {
                .initialized = [&](mwtl::WebView2InitializationResult) { late_callback = true; }}))
            return 14;
        cancelled.Close();
        static_cast<void>(PumpUntil(late_callback, std::chrono::seconds{1}));
        if (late_callback || cancelled.GetState() != mwtl::WebView2HostState::closed) return 15;
        std::error_code cancel_ignored;
        std::filesystem::remove_all(cancelled_data, cancel_ignored);
    }
    std::error_code ignored;
    std::filesystem::remove_all(data, ignored);
    return 0;
}
