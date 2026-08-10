#include <mwfl/webview2.h>

#include <objbase.h>
#include <oleacc.h>

#include <chrono>
#include <filesystem>

using mwfl::operator""_dip;

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

bool HasAccessibleName(HWND window, std::wstring_view expected) {
    IAccessible* accessible = nullptr;
    if (FAILED(::AccessibleObjectFromWindow(window, static_cast<DWORD>(OBJID_CLIENT),
            IID_IAccessible, reinterpret_cast<void**>(&accessible))) || accessible == nullptr)
        return false;
    VARIANT self{};
    self.vt = VT_I4;
    self.lVal = static_cast<LONG>(CHILDID_SELF);
    BSTR name = nullptr;
    const HRESULT read = accessible->get_accName(self, &name);
    const bool matches = SUCCEEDED(read) && name != nullptr &&
        std::wstring_view{name, ::SysStringLen(name)} == expected;
    if (name != nullptr) ::SysFreeString(name);
    accessible->Release();
    return matches;
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

    mwfl::WebView2Host host;
    if (!host.Create(parent, {1200}, {0.0_dip, 0.0_dip, 500.0_dip, 300.0_dip})) return 3;
    if (host.GetState() != mwfl::WebView2HostState::created || host.IsReady() ||
        !HasAccessibleName(host.GetHwnd(), L"Web content")) return 4;

    const auto runtime = mwfl::QueryWebView2Runtime();
    bool initialized = false;
    bool cycle_ready = false;
    bool navigated = false;
    int initialization_count = 0;
    mwfl::WebView2InitializationResult initialization{};
    const auto data = std::filesystem::temp_directory_path() /
        (L"mwfl-webview2-native-" + std::to_wstring(::GetCurrentProcessId()));
    if (!host.Initialize({.user_data_folder = data}, {
            .initialized = [&](mwfl::WebView2InitializationResult result) {
                initialization = result;
                initialized = true;
                ++initialization_count;
                cycle_ready = true;
                if (result.state == mwfl::WebView2HostState::ready)
                    static_cast<void>(host.NavigateToString(
                        L"<!doctype html><html><body><button autofocus>offline</button></body></html>"));
            },
            .navigation_completed = [&](bool succeeded, HRESULT) { navigated = succeeded; }})) return 5;
    if (!PumpUntil(initialized, std::chrono::seconds{20})) return 6;

    if (runtime.status == mwfl::WebView2RuntimeStatus::missing) {
        if (initialization.state != mwfl::WebView2HostState::failed ||
            initialization.runtime != mwfl::WebView2RuntimeStatus::missing) return 7;
    } else if (runtime.status == mwfl::WebView2RuntimeStatus::available) {
        if (initialization.state != mwfl::WebView2HostState::ready || !host.IsReady() ||
            host.GetController() == nullptr || host.GetWebView() == nullptr) return 8;
        if (!PumpUntil(navigated, std::chrono::seconds{15})) return 9;
        if (!host.SetBounds({5.0_dip, 6.0_dip, 420.0_dip, 240.0_dip}) || !host.Arrange() ||
            !host.FocusContent() || !host.Reload() || !host.Stop()) return 10;
        for (int cycle = 0; cycle < 5; ++cycle) {
            cycle_ready = false;
            navigated = false;
            if (!host.Restart() || !PumpUntil(cycle_ready, std::chrono::seconds{20}) ||
                !PumpUntil(navigated, std::chrono::seconds{15}) || !host.IsReady()) return 11;
        }
        if (initialization_count != 6) return 12;
    } else {
        if (initialization.state != mwfl::WebView2HostState::failed) return 13;
    }

    host.Close();
    if (host.GetState() != mwfl::WebView2HostState::closed || host.IsReady() ||
        host.GetController() != nullptr || host.GetWebView() != nullptr ||
        host.Navigate(L"https://example.invalid")) return 14;
    host.Close();

    if (runtime.status == mwfl::WebView2RuntimeStatus::available) {
        mwfl::WebView2Host cancelled;
        bool late_callback = false;
        const auto cancelled_data = data.wstring() + L"-cancelled";
        if (!cancelled.Create(parent, {1201}, {0.0_dip, 0.0_dip, 200.0_dip, 120.0_dip}) ||
            !cancelled.Initialize({.user_data_folder = cancelled_data}, {
                .initialized = [&](mwfl::WebView2InitializationResult) { late_callback = true; }}))
            return 15;
        cancelled.Close();
        static_cast<void>(PumpUntil(late_callback, std::chrono::seconds{1}));
        if (late_callback || cancelled.GetState() != mwfl::WebView2HostState::closed) return 16;
        std::error_code cancel_ignored;
        std::filesystem::remove_all(cancelled_data, cancel_ignored);
    }
    std::error_code ignored;
    std::filesystem::remove_all(data, ignored);
    return 0;
}
