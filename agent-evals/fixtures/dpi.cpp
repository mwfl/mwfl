#include <mwfl/mwfl.h>
using mwfl::operator""_dip;
class EvalDpi final : public mwfl::WindowBase {
public:
    void BuildUI() override {
        mwfl::ControlHost ui{*this}; ui.Add(label_, L"DPI aware"); ApplyFont(GetDpiContext().GetDpi());
        SetLayout(mwfl::Column().Margin(20.0_dip).Add(label_, mwfl::Stretch()));
    }
    mwfl::EventResult OnDpiChanged(const mwfl::DpiChangedEvent& event) override {
        ApplyFont(event.dpi_x); return mwfl::EventResult::Propagate();
    }
private:
    void ApplyFont(UINT dpi){ if(font_.CreateMessageFont(dpi)) mwfl::SetControlFont(label_.GetHwnd(),font_.GetHandle()); }
    mwfl::Label label_; mwfl::UiFont font_;
};

