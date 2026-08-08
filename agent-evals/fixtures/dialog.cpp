#include <mwtl/mwtl.h>
class EvalDialog final : public mwtl::WindowBase {
public:
    void BuildUI() override { mwtl::ControlHost ui{*this}; ui.Add(open_, L"Open"); ui.Add(result_, L""); }
    mwtl::EventResult OnCommand(const mwtl::CommandEvent& event) override {
        if (!event.IsClicked(open_)) return mwtl::EventResult::Propagate();
        mwtl::FileDialogOptions options{GetHwnd(), L"Open text"}; options.filters={{L"Text",L"*.txt"}};
        const auto value=mwtl::ShowOpenFileDialog(options);
        result_.SetText(value.accepted ? value.path.wstring() : (value.Cancelled() ? L"Cancelled" : L"Failed"));
        return mwtl::EventResult::Handled();
    }
private: mwtl::Button open_; mwtl::Label result_;
};

