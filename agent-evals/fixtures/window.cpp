#include <mwfl/mwfl.h>
using mwfl::operator""_dip;
class EvalWindow final : public mwfl::WindowBase {
public:
    void BuildUI() override {
        mwfl::ControlHost ui{*this}; ui.Add(status_, L"Ready"); ui.Add(close_, L"Close");
        SetLayout(mwfl::Column().Margin(16.0_dip).Gap(8.0_dip)
            .Add(status_, mwfl::Stretch()).Add(close_, mwfl::Fixed(36.0_dip)));
    }
    mwfl::EventResult OnCommand(const mwfl::CommandEvent& event) override {
        if (!event.IsClicked(close_)) return mwfl::EventResult::Propagate();
        Close(); return mwfl::EventResult::Handled();
    }
private: mwfl::Label status_; mwfl::Button close_;
};

