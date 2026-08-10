#include <mwfl/mwfl.h>
using mwfl::operator""_dip;
class EvalLayout final : public mwfl::WindowBase {
public:
    void BuildUI() override {
        mwfl::ControlHost ui{*this}; ui.Add(nav_, L"Navigation"); ui.Add(content_, L"Settings content");
        SetLayout(mwfl::Row().Margin(16.0_dip).Gap(12.0_dip)
            .Add(nav_, mwfl::Fixed(180.0_dip)).Add(content_, mwfl::Stretch(1.0f, 240.0_dip)));
    }
private: mwfl::GroupBox nav_, content_;
};

