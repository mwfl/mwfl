#include <mwfl/mwfl.h>
class EvalAppearance final : public mwfl::WindowBase {
public:
    void BuildUI() override {
        mwfl::ControlHost ui{*this}; ui.Add(input_, L"");
        mwfl::SetAccessibleName(input_.GetHwnd(), L"Search query");
        static_cast<void>(SetAppearance({mwfl::ColorMode::system}));
    }
private: mwfl::TextBox input_;
};

