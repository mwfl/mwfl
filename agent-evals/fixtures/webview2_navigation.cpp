#include <mwfl/mwfl.h>
#include <mwfl/webview2.h>

class WebViewFixture final : public mwfl::WindowBase {
public:
    void Start() {
        browser_.Initialize({}, {
            .initialized = [this](mwfl::WebView2InitializationResult result) {
                ready_ = result.state == mwfl::WebView2HostState::ready;
                if (ready_) browser_.NavigateToString(L"<p>offline</p>");
            },
            .process_failed = [this](mwfl::WebView2ProcessFailureKind) {
                ::PostMessageW(GetHwnd(), WM_APP + 1, 0, 0);
            },
            .accelerator_key = [this](const mwfl::WebView2AcceleratorKey& key) {
                if (key.message != WM_KEYDOWN || key.virtual_key != 'L') return false;
                address_.Focus();
                address_.SelectAll();
                return true;
            }});
    }

    void Navigate() {
        if (ready_) static_cast<void>(browser_.Navigate(address_.GetText()));
    }

    mwfl::EventResult OnMessage(const mwfl::WindowMessage& event) override {
        if (event.id != WM_APP + 1) return mwfl::EventResult::Propagate();
        static_cast<void>(browser_.Restart());
        return mwfl::EventResult::Handled();
    }

private:
    mwfl::WebView2Host browser_;
    mwfl::TextBox address_;
    bool ready_ = false;
};
