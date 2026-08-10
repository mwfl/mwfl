#include <mwfl/mwfl.h>

#include <string>

using mwfl::operator""_dip;

class FormWindow final : public mwfl::WindowBase {
public:
    void BuildUI() override {
        mwfl::ControlHost ui{*this};
        ui.Add(name_label_, L"Display name");
        ui.Add(name_, model_);
        ui.Add(validation_, L"");
        ui.Add(save_, L"Save");
        SetLayout(mwfl::Column().Margin(24.0_dip).Gap(8.0_dip)
            .Add(name_label_, mwfl::Auto())
            .Add(name_, mwfl::Fixed(34.0_dip))
            .Add(validation_, mwfl::Stretch())
            .Add(save_, mwfl::Fixed(38.0_dip)));
        name_.Focus();
    }

    mwfl::EventResult OnCommand(const mwfl::CommandEvent& event) override {
        if (!event.IsClicked(save_)) return mwfl::EventResult::Propagate();
        const std::wstring candidate = name_.GetText();
        if (candidate.size() < 2) {
            validation_.SetText(L"Enter at least two characters.");
            name_.Focus();
            name_.SelectAll();
        } else {
            model_ = candidate;
            validation_.SetText(L"Saved to the model.");
        }
        return mwfl::EventResult::Handled();
    }

private:
    std::wstring model_ = L"Ada";
    mwfl::Label name_label_, validation_;
    mwfl::TextBox name_;
    mwfl::Button save_;
};

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    return mwfl::RunApplication<FormWindow>(instance, show, {.title = L"Profile"});
}
