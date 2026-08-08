#include <mwtl/mwtl.h>
#include <string>
using mwtl::operator""_dip;
class EvalForm final : public mwtl::WindowBase {
public:
    void BuildUI() override {
        mwtl::ControlHost ui{*this}; ui.Add(name_, model_); ui.Add(result_, L""); ui.Add(save_, L"Save");
        SetLayout(mwtl::Column().Margin(16.0_dip).Gap(8.0_dip).Add(name_, 34.0_dip)
            .Add(result_, mwtl::Stretch()).Add(save_, 36.0_dip));
    }
    mwtl::EventResult OnCommand(const mwtl::CommandEvent& event) override {
        if (!event.IsClicked(save_)) return mwtl::EventResult::Propagate();
        const auto candidate = name_.GetText();
        if (candidate.size() < 2) { result_.SetText(L"Too short"); name_.Focus(); name_.SelectAll(); }
        else { model_ = candidate; result_.SetText(L"Saved"); }
        return mwtl::EventResult::Handled();
    }
private: std::wstring model_=L"Ada"; mwtl::TextBox name_; mwtl::Label result_; mwtl::Button save_;
};

