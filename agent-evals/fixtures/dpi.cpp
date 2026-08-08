#include <mwtl/mwtl.h>
using mwtl::operator""_dip;
class EvalDpi final : public mwtl::WindowBase {
public:
    void BuildUI() override {
        mwtl::ControlHost ui{*this}; ui.Add(label_, L"DPI aware"); ApplyFont(GetDpiContext().GetDpi());
        SetLayout(mwtl::Column().Margin(20.0_dip).Add(label_, mwtl::Stretch()));
    }
    mwtl::EventResult OnDpiChanged(const mwtl::DpiChangedEvent& event) override {
        ApplyFont(event.dpi_x); return mwtl::EventResult::Propagate();
    }
private:
    void ApplyFont(UINT dpi){ if(font_.CreateMessageFont(dpi)) mwtl::SetControlFont(label_.GetHwnd(),font_.GetHandle()); }
    mwtl::Label label_; mwtl::UiFont font_;
};

