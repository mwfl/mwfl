#include <mwfl/mwfl.h>

#include <cstdlib>

using mwfl::operator""_dip;

class MainWindow final : public mwfl::WindowBase {
public:
    void BuildUI() override {
        mwfl::Must(SetTitle(L"mwfl hello"), "set window title");
        mwfl::ControlHost ui{*this};
        ui.Add(
            message_,
            L"Hello from a native Windows UI with modern C++20 ergonomics.");
        ui.Add(button_, L"Make it brighter");
        SetLayout(
            mwfl::Column()
                .Margin(28.0_dip)
                .Gap(20.0_dip)
                .Add(message_, mwfl::Fixed(34.0_dip))
                .Add(button_, mwfl::Fixed(36.0_dip), {
                    .alignment = mwfl::CrossAlignment::start,
                    .preferred_size = mwfl::SizeDip{180.0_dip, 36.0_dip},
                }));
    }

    mwfl::EventResult OnCommand(const mwfl::CommandEvent& event) override {
        if (event.IsClicked(button_)) {
            message_.SetText(
                L"A real BUTTON HWND, dispatched without message-map macros.");
            return mwfl::EventResult::Handled();
        }
        return mwfl::EventResult::Propagate();
    }

private:
    mwfl::Label message_;
    mwfl::Button button_;
};

int WINAPI wWinMain(
    HINSTANCE instance,
    HINSTANCE,
    PWSTR,
    int show_command) {
    return mwfl::RunApplication<MainWindow>(
        instance,
        show_command,
        {
            .initial_bounds = {{0.0_dip, 0.0_dip}, {960.0_dip, 320.0_dip}},
            .use_default_bounds = false,
        });
}
