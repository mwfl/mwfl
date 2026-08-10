#include <mwfl/mwfl.h>

using mwfl::operator""_dip;

class MainWindow final : public mwfl::WindowBase {
public:
    void BuildUI() override {
        SetTitle(L"mwfl starter");
        mwfl::ControlHost ui{*this};
        ui.Add(message_, L"A native Windows application generated from the mwfl starter.");
        ui.Add(close_, L"Close");
        SetLayout(mwfl::Column().Margin(24.0_dip).Gap(12.0_dip)
            .Add(message_, mwfl::Stretch())
            .Add(close_, mwfl::Fixed(38.0_dip)));
    }

    mwfl::EventResult OnCommand(const mwfl::CommandEvent& event) override {
        if (event.IsClicked(close_)) {
            Close();
            return mwfl::EventResult::Handled();
        }
        return mwfl::EventResult::Propagate();
    }

private:
    mwfl::Label message_;
    mwfl::Button close_;
};

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    return mwfl::RunApplication<MainWindow>(instance, show);
}

