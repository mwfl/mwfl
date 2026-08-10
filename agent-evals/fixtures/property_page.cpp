#include <mwtl/mwtl.h>

#include <vector>

namespace {

class PropertyPageEval final : public mwtl::WindowBase {
public:
    void BuildUI() override {
        mwtl::ControlHost ui{*this};
        ui.Add(open_, L"Settings");
    }

    mwtl::EventResult OnCommand(const mwtl::CommandEvent& event) override {
        if (!event.IsClicked(open_)) return mwtl::EventResult::Propagate();
        pages_.clear();
        pages_.emplace_back(mwtl::PropertyPageOptions{
            {1}, L"Profile",
            {.initialize = [this](HWND page) {
                 mwtl::ControlHost ui{page};
                 ui.Add(name_, {101}, committed_name_, {});
                 return pages_[0].SetLayout(
                     mwtl::Column().Margin(mwtl::Dip{12}).Add(name_, mwtl::Fixed(mwtl::Dip{32})));
             },
             .command = [this](HWND, WORD id, WORD code) {
                 return id == 101 && code == EN_CHANGE && pages_[0].SetDirty();
             },
             .validate = [this](HWND) {
                 return name_.GetText().empty() ? mwtl::PropertyPageValidation::invalid
                                                : mwtl::PropertyPageValidation::valid;
             },
             .apply = [this](HWND) {
                 committed_name_ = name_.GetText();
                 return true;
             },
             .reset = [this](HWND) { name_.SetText(committed_name_); }}});
        sheet_.CreateModeless({.owner = GetHwnd(), .title = L"Settings"}, pages_);
        return mwtl::EventResult::Handled();
    }

private:
    std::wstring committed_name_ = L"Ada";
    mwtl::Button open_;
    mwtl::TextBox name_;
    std::vector<mwtl::PropertyPage> pages_;
    mwtl::PropertySheetDialog sheet_;
};

}  // namespace
