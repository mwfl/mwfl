#include <mwtl/mwtl.h>
#include <mwtl/webview2.h>

#include <filesystem>
#include <string>

using mwtl::operator""_dip;

namespace {
constexpr mwtl::ControlId kBack{950};
constexpr mwtl::ControlId kForward{951};
constexpr mwtl::ControlId kReload{952};
constexpr mwtl::ControlId kAddress{953};
constexpr mwtl::ControlId kGo{954};
constexpr mwtl::ControlId kBrowser{955};
constexpr UINT kRestartBrowser = WM_APP + 0x1A0;
constexpr UINT kFinishSelfTest = WM_APP + 0x1A1;

class BrowserWindow final : public mwtl::WindowBase {
public:
    void BuildUI() override {
        SetTitle(L"mwtl Browser");
        mwtl::ControlHost ui{*this};
        ui.Add(back_, kBack, L"Back");
        ui.Add(forward_, kForward, L"Forward");
        ui.Add(reload_, kReload, L"Reload");
        ui.Add(address_, kAddress, L"https://example.com");
        ui.Add(go_, kGo, L"Go");
        ui.AddNative(browser_, kBrowser, mwtl::RectDip{});
        ui.Add(status_, L"Starting WebView2...");

        mwtl::Must(mwtl::SetAccessibleName(address_.GetHwnd(), L"Web address"),
                   "name browser address field");
        mwtl::Must(mwtl::SetAccessibleName(browser_.GetHwnd(), L"Web page"),
                   "name browser content");
        mwtl::Must(mwtl::SetAccessibleName(status_.GetHwnd(), L"Browser status"),
                   "name browser status");
        SetLayout(mwtl::Column().Gap(6.0_dip).Margin(8.0_dip)
            .Add(mwtl::Row().Gap(6.0_dip)
                .Add(back_, mwtl::Fixed(68.0_dip))
                .Add(forward_, mwtl::Fixed(78.0_dip))
                .Add(reload_, mwtl::Fixed(76.0_dip))
                .Add(address_, mwtl::Stretch())
                .Add(go_, mwtl::Fixed(58.0_dip)), mwtl::Fixed(30.0_dip))
            .Add(browser_, mwtl::Stretch()).Add(status_, mwtl::Auto()));

        StartBrowser();
    }

    mwtl::EventResult OnCommand(const mwtl::CommandEvent& event) override {
        if (event.IsClicked(back_)) static_cast<void>(browser_.GoBack());
        else if (event.IsClicked(forward_)) static_cast<void>(browser_.GoForward());
        else if (event.IsClicked(reload_)) static_cast<void>(browser_.Reload());
        else if (event.IsClicked(go_)) NavigateAddress();
        else return mwtl::EventResult::Propagate();
        return mwtl::EventResult::Handled();
    }

    mwtl::EventResult OnMessage(const mwtl::WindowMessage& event) override {
        if (event.id == kRestartBrowser) {
            status_.SetText(L"Recovering WebView2 process...");
            if (!browser_.Restart()) status_.SetText(L"WebView2 recovery failed");
            return mwtl::EventResult::Handled();
        }
        if (event.id == kFinishSelfTest) {
            RunSelfTest();
            return mwtl::EventResult::Handled();
        }
        return mwtl::EventResult::Propagate();
    }

    mwtl::EventResult OnClose() override {
        browser_.Close();
        if (self_test_) {
            std::error_code ignored;
            std::filesystem::remove_all(user_data_folder_, ignored);
        }
        return mwtl::EventResult::Propagate();
    }

private:
    void StartBrowser() {
        self_test_ = std::wstring_view{::GetCommandLineW()}.find(L"--self-test") !=
                     std::wstring_view::npos;
        if (self_test_) {
            user_data_folder_ = std::filesystem::temp_directory_path() /
                (L"mwtl-browser-" + std::to_wstring(::GetCurrentProcessId()));
        }
        const bool started = browser_.Initialize({.user_data_folder = user_data_folder_}, {
            .initialized = [this](mwtl::WebView2InitializationResult result) {
                if (result.state != mwtl::WebView2HostState::ready) {
                    status_.SetText(result.runtime == mwtl::WebView2RuntimeStatus::missing
                        ? L"WebView2 Runtime is missing. Install Microsoft Edge WebView2 Evergreen Runtime."
                        : L"WebView2 initialization failed.");
                    if (self_test_) ::PostQuitMessage(
                        result.runtime == mwtl::WebView2RuntimeStatus::missing ? 0 : 1);
                    return;
                }
                status_.SetText(L"Ready | Ctrl+L focuses the address bar");
                if (self_test_) {
                    static_cast<void>(browser_.NavigateToString(
                        L"<!doctype html><html><body><h1>mwtl offline self-test</h1></body></html>"));
                } else {
                    static_cast<void>(browser_.NavigateToString(
                        L"<!doctype html><html><body><h1>mwtl Browser</h1>"
                        L"<p>Enter an address above. This welcome page is offline.</p></body></html>"));
                }
            },
            .navigation_completed = [this](bool succeeded, HRESULT) {
                navigation_succeeded_ = succeeded;
                status_.SetText(succeeded ? L"Navigation complete" : L"Navigation failed");
                if (self_test_ && ::PostMessageW(GetHwnd(), kFinishSelfTest, 0, 0) == FALSE)
                    ::PostQuitMessage(2);
            },
            .process_failed = [this](mwtl::WebView2ProcessFailureKind kind) {
                status_.SetText(L"WebView2 process failed; recovery scheduled");
                if (kind == mwtl::WebView2ProcessFailureKind::render_process_unresponsive) {
                    static_cast<void>(browser_.Reload());
                } else if (::PostMessageW(GetHwnd(), kRestartBrowser, 0, 0) == FALSE) {
                    status_.SetText(L"WebView2 recovery could not be scheduled");
                }
            },
            .accelerator_key = [this](const mwtl::WebView2AcceleratorKey& key) {
                return HandleAccelerator(key);
            }});
        if (!started) {
            status_.SetText(L"WebView2 initialization could not start");
            if (self_test_) ::PostQuitMessage(3);
        }
    }

    bool HandleAccelerator(const mwtl::WebView2AcceleratorKey& key) {
        if (key.message == WM_KEYDOWN && key.virtual_key == 'L' &&
            (::GetKeyState(VK_CONTROL) & 0x8000) != 0) {
            address_.Focus();
            address_.SelectAll();
            return true;
        }
        return false;
    }

    void NavigateAddress() {
        std::wstring uri = address_.GetText();
        if (uri.find(L"://") == std::wstring::npos) uri = L"https://" + uri;
        if (!browser_.Navigate(uri)) status_.SetText(L"Navigation could not start");
    }

    void RunSelfTest() noexcept {
        int result = navigation_succeeded_ ? 0 : 4;
        if (result == 0 && (!browser_.IsReady() || browser_.GetController() == nullptr ||
                            browser_.GetWebView() == nullptr)) result = 5;
        if (result == 0 && (!browser_.SetBounds({4.0_dip, 5.0_dip, 500.0_dip, 280.0_dip}) ||
                            !browser_.Arrange() || !browser_.FocusContent())) result = 6;
        const mwtl::WebView2AcceleratorKey ordinary{WM_KEYDOWN, 'X', 0};
        if (result == 0 && HandleAccelerator(ordinary)) result = 7;
        browser_.Close();
        if (result == 0 && browser_.GetState() != mwtl::WebView2HostState::closed) result = 8;
        ::PostQuitMessage(result);
    }

    mwtl::Button back_, forward_, reload_, go_;
    mwtl::TextBox address_;
    mwtl::WebView2Host browser_;
    mwtl::Label status_;
    std::filesystem::path user_data_folder_;
    bool self_test_ = false;
    bool navigation_succeeded_ = false;
};
}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
    return mwtl::RunApplication<BrowserWindow>(
        instance, show_command, {}, {.com_apartment = mwtl::ComApartment::sta});
}
