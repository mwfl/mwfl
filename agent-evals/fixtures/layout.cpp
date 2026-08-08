#include <mwtl/mwtl.h>
using mwtl::operator""_dip;
class EvalLayout final : public mwtl::WindowBase {
public:
    void BuildUI() override {
        mwtl::ControlHost ui{*this}; ui.Add(nav_, L"Navigation"); ui.Add(content_, L"Settings content");
        SetLayout(mwtl::Row().Margin(16.0_dip).Gap(12.0_dip)
            .Add(nav_, mwtl::Fixed(180.0_dip)).Add(content_, mwtl::Stretch(1.0f, 240.0_dip)));
    }
private: mwtl::GroupBox nav_, content_;
};

