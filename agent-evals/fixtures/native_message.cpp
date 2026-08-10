#include <mwfl/mwfl.h>
class EvalNativeMessage final : public mwfl::WindowBase {
public:
    void BuildUI() override { ::PostMessageW(GetHwnd(), kMessage, 0, 0); }
    mwfl::EventResult OnMessage(const mwfl::WindowMessage& event) override {
        if(event.id!=kMessage) return mwfl::EventResult::Propagate();
        SetTitle(L"Native message received"); return mwfl::EventResult::Handled();
    }
private: static constexpr UINT kMessage=WM_APP+42;
};

