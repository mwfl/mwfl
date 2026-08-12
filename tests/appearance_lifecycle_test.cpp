#include <mwfl/mwfl.h>

#include <stdexcept>

namespace {

class AppearanceLifecycleWindow final : public mwfl::WindowBase {
public:
    void BuildUI() override {
        mwfl::ControlHost ui{*this};
        ui.Add(label_, L"Theme-aware native control");
        if (!menu_.Create() || !menu_.AppendCommand(100, L"Theme command") ||
            !menu_.AttachToWindow(GetHwnd()))
            throw std::runtime_error("create themed menu failed");
        if (::SetTimer(GetHwnd(), 1, 1, nullptr) == 0)
            throw std::runtime_error("start appearance timer failed");
    }

    mwfl::EventResult OnTimer(mwfl::TimerId id) override {
        if (id.value != 1) return mwfl::EventResult::Propagate();
        ::KillTimer(GetHwnd(), 1);
        ::SendMessageW(GetHwnd(), WM_SETTINGCHANGE, 0, 0);
        return mwfl::EventResult::Handled();
    }

    mwfl::EventResult OnAppearanceChanged(const mwfl::AppearanceState& state) override {
        const auto child_state = mwfl::GetWindowAppearance(label_.GetHwnd());
        const bool expected_dark = !mwfl::IsHighContrastEnabled();
        const bool valid = state.requested_mode == mwfl::ColorMode::dark &&
                           state.IsDark() == expected_dark &&
                           child_state.IsDark() == expected_dark &&
                           state.palette.text != state.palette.window_background;
        if (!valid) {
            ::PostQuitMessage(7);
        } else if (::PostMessageW(GetHwnd(), WM_CLOSE, 0, 0) == FALSE) {
            ::PostQuitMessage(8);
        }
        return mwfl::EventResult::Propagate();
    }

private:
    mwfl::Label label_;
    mwfl::Menu menu_;
};

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    mwfl::WindowOptions options;
    options.title = L"Appearance lifecycle test";
    options.appearance.color_mode = mwfl::ColorMode::dark;
    return mwfl::RunApplication<AppearanceLifecycleWindow>(instance, SW_HIDE, options);
}
