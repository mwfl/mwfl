#include <mwfl/mwfl.h>

#include <vector>

namespace {

class PropertyPageEval final : public mwfl::WindowBase {
public:
    void BuildUI() override {
        mwfl::ControlHost ui{*this};
        ui.Add(open_, L"Settings");
    }

    mwfl::EventResult OnCommand(const mwfl::CommandEvent& event) override {
        if (!event.IsClicked(open_)) return mwfl::EventResult::Propagate();
        pages_.clear();
        pages_.emplace_back(mwfl::PropertyPageOptions{
            {1}, L"Profile",
            {.initialize = [this](HWND page) {
                 mwfl::ControlHost ui{page};
                 ui.Add(name_, {101}, committed_name_, {});
                 return pages_[0].SetLayout(
                     mwfl::Column().Margin(mwfl::Dip{12}).Add(name_, mwfl::Fixed(mwfl::Dip{32})));
             },
             .command = [this](HWND, WORD id, WORD code) {
                 return id == 101 && code == EN_CHANGE && pages_[0].SetDirty();
             },
             .validate = [this](HWND) {
                 return name_.GetText().empty() ? mwfl::PropertyPageValidation::invalid
                                                : mwfl::PropertyPageValidation::valid;
             },
             .apply = [this](HWND) {
                 committed_name_ = name_.GetText();
                 return true;
             },
             .reset = [this](HWND) { name_.SetText(committed_name_); }}});
        sheet_.CreateModeless({.owner = GetHwnd(), .title = L"Settings"}, pages_);
        return mwfl::EventResult::Handled();
    }

private:
    std::wstring committed_name_ = L"Ada";
    mwfl::Button open_;
    mwfl::TextBox name_;
    std::vector<mwfl::PropertyPage> pages_;
    mwfl::PropertySheetDialog sheet_;
};

}  // namespace
