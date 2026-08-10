#include <mwtl/mwtl.h>

using mwtl::operator""_dip;

class NativeHostFixture final : public mwtl::WindowBase {
public:
    void BuildUI() override {
        mwtl::ControlHost ui{*this};
        ui.Add(host_);
        child_ = ::CreateWindowExW(0, L"STATIC", L"Third-party child",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 1, 1,
            host_.GetHwnd(), nullptr, nullptr, nullptr);
        mwtl::Must(child_ != nullptr && host_.Attach(child_), "attach native child");
        SetLayout(mwtl::Column().Margin(8.0_dip).Add(host_, mwtl::Stretch()));
    }

    mwtl::EventResult OnClose() override {
        if (host_.GetChild() != nullptr) {
            const HWND detached = host_.Detach();
            if (detached != nullptr) ::DestroyWindow(detached);
            child_ = nullptr;
        }
        return mwtl::EventResult::Propagate();
    }

private:
    mwtl::NativeHost host_;
    HWND child_ = nullptr;
};
