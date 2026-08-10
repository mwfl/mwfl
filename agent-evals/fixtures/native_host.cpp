#include <mwfl/mwfl.h>

using mwfl::operator""_dip;

class NativeHostFixture final : public mwfl::WindowBase {
public:
    void BuildUI() override {
        mwfl::ControlHost ui{*this};
        ui.Add(host_);
        child_ = ::CreateWindowExW(0, L"STATIC", L"Third-party child",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 1, 1,
            host_.GetHwnd(), nullptr, nullptr, nullptr);
        mwfl::Must(child_ != nullptr && host_.Attach(child_), "attach native child");
        SetLayout(mwfl::Column().Margin(8.0_dip).Add(host_, mwfl::Stretch()));
    }

    mwfl::EventResult OnClose() override {
        if (host_.GetChild() != nullptr) {
            const HWND detached = host_.Detach();
            if (detached != nullptr) ::DestroyWindow(detached);
            child_ = nullptr;
        }
        return mwfl::EventResult::Propagate();
    }

private:
    mwfl::NativeHost host_;
    HWND child_ = nullptr;
};
