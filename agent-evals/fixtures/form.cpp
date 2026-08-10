#include <mwfl/mwfl.h>
#include <string>
using mwfl::operator""_dip;
class EvalForm final : public mwfl::WindowBase {
public:
    void BuildUI() override {
        mwfl::ControlHost ui{*this}; ui.Add(name_, model_); ui.Add(result_, L""); ui.Add(save_, L"Save");
        SetLayout(mwfl::Column().Margin(16.0_dip).Gap(8.0_dip).Add(name_, 34.0_dip)
            .Add(result_, mwfl::Stretch()).Add(save_, 36.0_dip));
    }
    mwfl::EventResult OnCommand(const mwfl::CommandEvent& event) override {
        if (!event.IsClicked(save_)) return mwfl::EventResult::Propagate();
        const auto candidate = name_.GetText();
        if (candidate.size() < 2) { result_.SetText(L"Too short"); name_.Focus(); name_.SelectAll(); }
        else { model_ = candidate; result_.SetText(L"Saved"); }
        return mwfl::EventResult::Handled();
    }
private: std::wstring model_=L"Ada"; mwfl::TextBox name_; mwfl::Label result_; mwfl::Button save_;
};

