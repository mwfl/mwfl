#include <mwfl/mwfl.h>

#include <chrono>
#include <concepts>
#include <string_view>

using mwfl::operator""_dip;

// Compile-only contract for the representative v0.1 public forms. Changing one
// of these forms requires the documented compatibility process.
class PublicBaselineWindow final : public mwfl::WindowBase {
public:
    void BuildUI() override {
        mwfl::ControlHost ui{*this};
        ui.Add(label_, L"Name");
        ui.Add(button_, L"Save");
        SetLayout(mwfl::Column().Margin(16.0_dip).Gap(8.0_dip)
            .Add(label_, mwfl::Auto()).Add(button_, mwfl::Fixed(32.0_dip)));
        mwfl::Must(timer_.Start(*this, {1}, std::chrono::milliseconds(100)),
                   "public baseline timer");
    }

    mwfl::EventResult OnCommand(const mwfl::CommandEvent& event) override {
        return event.IsClicked(button_) ? mwfl::EventResult::Handled()
                                        : mwfl::EventResult::Propagate();
    }

private:
    mwfl::Label label_;
    mwfl::Button button_;
    mwfl::UiTimer timer_;
};

static_assert(mwfl::MainWindow<PublicBaselineWindow>);
static_assert(mwfl::WindowLike<PublicBaselineWindow>);
static_assert(mwfl::ControlLike<mwfl::Button>);
static_assert(requires(mwfl::Button& button, HWND parent) {
    { button.Create(parent, {42}, std::wstring_view{L"Save"},
                    mwfl::RectDip{}) } -> std::same_as<bool>;
    { button.GetHwnd() } -> std::same_as<HWND>;
    { button.SetText(std::wstring_view{}) } -> std::same_as<bool>;
});

int CompilePublicBaseline(HINSTANCE instance, int show) {
    mwfl::WindowOptions options;
    options.use_system_message_font = true;
    return mwfl::RunApplication<PublicBaselineWindow>(instance, show, options);
}
