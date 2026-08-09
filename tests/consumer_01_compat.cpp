#include <mwtl/mwtl.h>

#include <chrono>
#include <concepts>
#include <string_view>

using mwtl::operator""_dip;

// Compile-only contract for representative stable 0.1 source forms. Removing
// or changing one of these requires a deprecation period and migration entry.
class CompatWindow final : public mwtl::WindowBase {
public:
    void BuildUI() override {
        mwtl::ControlHost ui{*this};
        ui.Add(label_, L"Name");
        ui.Add(button_, L"Save");
        SetLayout(mwtl::Column().Margin(16.0_dip).Gap(8.0_dip)
            .Add(label_, mwtl::Auto()).Add(button_, mwtl::Fixed(32.0_dip)));
        mwtl::Must(timer_.Start(*this, {1}, std::chrono::milliseconds(100)),
                   "compat timer");
    }

    mwtl::EventResult OnCommand(const mwtl::CommandEvent& event) override {
        return event.IsClicked(button_) ? mwtl::EventResult::Handled()
                                        : mwtl::EventResult::Propagate();
    }

private:
    mwtl::Label label_;
    mwtl::Button button_;
    mwtl::UiTimer timer_;
};

static_assert(mwtl::MainWindow<CompatWindow>);
static_assert(mwtl::WindowLike<CompatWindow>);
static_assert(mwtl::ControlLike<mwtl::Button>);
static_assert(requires(mwtl::Button& button, HWND parent) {
    { button.Create(parent, {42}, std::wstring_view{L"Save"},
                    mwtl::RectDip{}) } -> std::same_as<bool>;
    { button.GetHwnd() } -> std::same_as<HWND>;
    { button.SetText(std::wstring_view{}) } -> std::same_as<bool>;
});

int Compile01Run(HINSTANCE instance, int show) {
    return mwtl::RunApplication<CompatWindow>(instance, show);
}
