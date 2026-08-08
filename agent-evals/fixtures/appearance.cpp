#include <mwtl/mwtl.h>
class EvalAppearance final : public mwtl::WindowBase {
public:
    void BuildUI() override {
        mwtl::ControlHost ui{*this}; ui.Add(input_, L"");
        mwtl::SetAccessibleName(input_.GetHwnd(), L"Search query");
        static_cast<void>(mwtl::ApplyWindowAppearance(GetHwnd(), {mwtl::ColorMode::system}));
    }
private: mwtl::TextBox input_;
};

