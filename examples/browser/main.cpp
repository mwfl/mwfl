#include <mwfl/mwfl.h>
#include <mwfl/webview2.h>

#include <filesystem>
#include <string>

using mwfl::operator""_dip;

namespace {
constexpr mwfl::ControlId kBack{950};
constexpr mwfl::ControlId kForward{951};
constexpr mwfl::ControlId kReload{952};
constexpr mwfl::ControlId kAddress{953};
constexpr mwfl::ControlId kGo{954};
constexpr mwfl::ControlId kBrowser{955};
constexpr UINT kRestartBrowser = WM_APP + 0x1A0;
constexpr UINT kFinishSelfTest = WM_APP + 0x1A1;

class BrowserWindow final : public mwfl::WindowBase {
public:
    void BuildUI() override {
        SetTitle(L"mwfl Browser");
        mwfl::ControlHost ui{*this};
        ui.Add(back_, kBack, L"Back");
        ui.Add(forward_, kForward, L"Forward");
        ui.Add(reload_, kReload, L"Reload");
        ui.Add(address_, kAddress, L"https://example.com");
        ui.Add(go_, kGo, L"Go");
        ui.AddNative(browser_, kBrowser, mwfl::RectDip{});
        ui.Add(status_, L"Starting WebView2...");

        mwfl::Must(mwfl::SetAccessibleName(address_.GetHwnd(), L"Web address"),
                   "name browser address field");
        mwfl::Must(mwfl::SetAccessibleName(browser_.GetHwnd(), L"Web page"),
                   "name browser content");
        mwfl::Must(mwfl::SetAccessibleName(status_.GetHwnd(), L"Browser status"),
                   "name browser status");
        SetLayout(mwfl::Column().Gap(6.0_dip).Margin(8.0_dip)
            .Add(mwfl::Row().Gap(6.0_dip)
                .Add(back_, mwfl::Fixed(68.0_dip))
                .Add(forward_, mwfl::Fixed(78.0_dip))
                .Add(reload_, mwfl::Fixed(76.0_dip))
                .Add(address_, mwfl::Stretch())
                .Add(go_, mwfl::Fixed(58.0_dip)), mwfl::Fixed(30.0_dip))
            .Add(browser_, mwfl::Stretch()).Add(status_, mwfl::Auto()));

        StartBrowser();
    }

    mwfl::EventResult OnCommand(const mwfl::CommandEvent& event) override {
        if (event.IsClicked(back_)) static_cast<void>(browser_.GoBack());
        else if (event.IsClicked(forward_)) static_cast<void>(browser_.GoForward());
        else if (event.IsClicked(reload_)) static_cast<void>(browser_.Reload());
        else if (event.IsClicked(go_)) NavigateAddress();
        else return mwfl::EventResult::Propagate();
        return mwfl::EventResult::Handled();
    }

    mwfl::EventResult OnMessage(const mwfl::WindowMessage& event) override {
        if (event.id == kRestartBrowser) {
            status_.SetText(L"Recovering WebView2 process...");
            if (!browser_.Restart()) status_.SetText(L"WebView2 recovery failed");
            return mwfl::EventResult::Handled();
        }
        if (event.id == kFinishSelfTest) {
            RunSelfTest();
            return mwfl::EventResult::Handled();
        }
        return mwfl::EventResult::Propagate();
    }

    mwfl::EventResult OnClose() override {
        browser_.Close();
        if (self_test_) {
            std::error_code ignored;
            std::filesystem::remove_all(user_data_folder_, ignored);
        }
        return mwfl::EventResult::Propagate();
    }

private:
    void StartBrowser() {
        self_test_ = std::wstring_view{::GetCommandLineW()}.find(L"--self-test") !=
                     std::wstring_view::npos;
        if (self_test_) {
            user_data_folder_ = std::filesystem::temp_directory_path() /
                (L"mwfl-browser-" + std::to_wstring(::GetCurrentProcessId()));
        }
        const bool started = browser_.Initialize({.user_data_folder = user_data_folder_}, {
            .initialized = [this](mwfl::WebView2InitializationResult result) {
                if (result.state != mwfl::WebView2HostState::ready) {
                    status_.SetText(result.runtime == mwfl::WebView2RuntimeStatus::missing
                        ? L"WebView2 Runtime is missing. Install Microsoft Edge WebView2 Evergreen Runtime."
                        : L"WebView2 initialization failed.");
                    if (self_test_) ::PostQuitMessage(
                        result.runtime == mwfl::WebView2RuntimeStatus::missing ? 0 : 1);
                    return;
                }
                status_.SetText(L"Ready | Ctrl+L focuses the address bar");
                if (self_test_) {
                    static_cast<void>(browser_.NavigateToString(
                        L"<!doctype html><html><body><h1>mwfl offline self-test</h1></body></html>"));
                } else {
                    static_cast<void>(browser_.NavigateToString(
                        L"<!doctype html><html><body><h1>mwfl Browser</h1>"
                        L"<p>Enter an address above. This welcome page is offline.</p></body></html>"));
                }
            },
            .navigation_completed = [this](bool succeeded, HRESULT) {
                navigation_succeeded_ = succeeded;
                status_.SetText(succeeded ? L"Navigation complete" : L"Navigation failed");
                if (self_test_ && ::PostMessageW(GetHwnd(), kFinishSelfTest, 0, 0) == FALSE)
                    ::PostQuitMessage(2);
            },
            .process_failed = [this](mwfl::WebView2ProcessFailureKind kind) {
                status_.SetText(L"WebView2 process failed; recovery scheduled");
                if (kind == mwfl::WebView2ProcessFailureKind::render_process_unresponsive) {
                    static_cast<void>(browser_.Reload());
                } else if (::PostMessageW(GetHwnd(), kRestartBrowser, 0, 0) == FALSE) {
                    status_.SetText(L"WebView2 recovery could not be scheduled");
                }
            },
            .accelerator_key = [this](const mwfl::WebView2AcceleratorKey& key) {
                return HandleAccelerator(key);
            }});
        if (!started) {
            status_.SetText(L"WebView2 initialization could not start");
            if (self_test_) ::PostQuitMessage(3);
        }
    }

    bool HandleAccelerator(const mwfl::WebView2AcceleratorKey& key) {
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
        const mwfl::WebView2AcceleratorKey ordinary{WM_KEYDOWN, 'X', 0};
        if (result == 0 && HandleAccelerator(ordinary)) result = 7;
        browser_.Close();
        if (result == 0 && browser_.GetState() != mwfl::WebView2HostState::closed) result = 8;
        ::PostQuitMessage(result);
    }

    mwfl::Button back_, forward_, reload_, go_;
    mwfl::TextBox address_;
    mwfl::WebView2Host browser_;
    mwfl::Label status_;
    std::filesystem::path user_data_folder_;
    bool self_test_ = false;
    bool navigation_succeeded_ = false;
};
}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
    return mwfl::RunApplication<BrowserWindow>(
        instance, show_command, {}, {.com_apartment = mwfl::ComApartment::sta});
}
