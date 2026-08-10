#include <mwtl/mwtl.h>
#include <mwtl/webview2.h>

class WebViewFixture final : public mwtl::WindowBase {
public:
    void Start() {
        browser_.Initialize({}, {
            .initialized = [this](mwtl::WebView2InitializationResult result) {
                ready_ = result.state == mwtl::WebView2HostState::ready;
                if (ready_) browser_.NavigateToString(L"<p>offline</p>");
            },
            .process_failed = [this](mwtl::WebView2ProcessFailureKind) {
                ::PostMessageW(GetHwnd(), WM_APP + 1, 0, 0);
            },
            .accelerator_key = [this](const mwtl::WebView2AcceleratorKey& key) {
                if (key.message != WM_KEYDOWN || key.virtual_key != 'L') return false;
                address_.Focus();
                address_.SelectAll();
                return true;
            }});
    }

    void Navigate() {
        if (ready_) static_cast<void>(browser_.Navigate(address_.GetText()));
    }

    mwtl::EventResult OnMessage(const mwtl::WindowMessage& event) override {
        if (event.id != WM_APP + 1) return mwtl::EventResult::Propagate();
        static_cast<void>(browser_.Restart());
        return mwtl::EventResult::Handled();
    }

private:
    mwtl::WebView2Host browser_;
    mwtl::TextBox address_;
    bool ready_ = false;
};
