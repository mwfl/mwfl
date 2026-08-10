#include <mwfl/mwfl.h>

#include <type_traits>

using mwfl::operator""_dip;

static_assert(!std::is_copy_constructible_v<mwfl::NativeControl>);
static_assert(std::is_copy_constructible_v<mwfl::WindowWakeup>);

class ProbeWindow final : public mwfl::WindowBase {
public:
    void BuildUI() override {
        mwfl::ControlHost ui{*this};
        ui.Add(text_, L"Public API probe");
        ui.Add(button_, L"Close");
        SetLayout(mwfl::Column().Margin(12.0_dip).Gap(8.0_dip)
            .Add(text_, mwfl::Stretch())
            .Add(button_, mwfl::Fixed(34.0_dip)));
    }

    mwfl::EventResult OnCommand(const mwfl::CommandEvent& event) override {
        if (!event.IsClicked(button_)) return mwfl::EventResult::Propagate();
        Close();
        return mwfl::EventResult::Handled();
    }

private:
    mwfl::Label text_;
    mwfl::Button button_;
};

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    return mwfl::RunApplication<ProbeWindow>(instance, show);
}

