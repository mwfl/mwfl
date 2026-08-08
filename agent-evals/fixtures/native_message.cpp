#include <mwtl/mwtl.h>
class EvalNativeMessage final : public mwtl::WindowBase {
public:
    void BuildUI() override { ::PostMessageW(GetHwnd(), kMessage, 0, 0); }
    mwtl::EventResult OnMessage(const mwtl::WindowMessage& event) override {
        if(event.id!=kMessage) return mwtl::EventResult::Propagate();
        SetTitle(L"Native message received"); return mwtl::EventResult::Handled();
    }
private: static constexpr UINT kMessage=WM_APP+42;
};

