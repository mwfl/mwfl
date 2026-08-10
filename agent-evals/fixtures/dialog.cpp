#include <mwfl/mwfl.h>
class EvalDialog final : public mwfl::WindowBase {
public:
    void BuildUI() override { mwfl::ControlHost ui{*this}; ui.Add(open_, L"Open"); ui.Add(result_, L""); }
    mwfl::EventResult OnCommand(const mwfl::CommandEvent& event) override {
        if (!event.IsClicked(open_)) return mwfl::EventResult::Propagate();
        mwfl::FileDialogOptions options{GetHwnd(), L"Open text"}; options.filters={{L"Text",L"*.txt"}};
        const auto value=mwfl::ShowOpenFileDialog(options);
        result_.SetText(value.accepted ? value.path.wstring() : (value.Cancelled() ? L"Cancelled" : L"Failed"));
        return mwfl::EventResult::Handled();
    }
private: mwfl::Button open_; mwfl::Label result_;
};

