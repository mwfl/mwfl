#include <mwtl/mwtl.h>
using mwtl::operator""_dip;
class EvalWindow final : public mwtl::WindowBase {
public:
    void BuildUI() override {
        mwtl::ControlHost ui{*this}; ui.Add(status_, L"Ready"); ui.Add(close_, L"Close");
        SetLayout(mwtl::Column().Margin(16.0_dip).Gap(8.0_dip)
            .Add(status_, mwtl::Stretch()).Add(close_, mwtl::Fixed(36.0_dip)));
    }
    mwtl::EventResult OnCommand(const mwtl::CommandEvent& event) override {
        if (!event.IsClicked(close_)) return mwtl::EventResult::Propagate();
        Close(); return mwtl::EventResult::Handled();
    }
private: mwtl::Label status_; mwtl::Button close_;
};

