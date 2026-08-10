#include <mwfl/mwfl.h>

#include <array>
#include <string>

using mwfl::operator""_dip;

namespace {

class AppearanceWindow final : public mwfl::WindowBase {
public:
    void BuildUI() override {
        SetTitle(L"Appearance lab");
        mwfl::ControlHost ui{*this};
        ui.Add(title_, L"Appearance lab");
        ui.Add(subtitle_, L"Explore Windows 11 title-bar color, backdrop material, corner policy, and accessibility helpers.");
        ui.Add(settings_, L"Window composition");
        ui.Add(mode_label_, L"Color mode");
        ui.Add(mode_);
        ui.Add(backdrop_label_, L"Backdrop material");
        ui.Add(backdrop_);
        ui.Add(rounded_, L"Prefer rounded corners");
        ui.Add(apply_, L"Apply appearance");
        ui.Add(preview_, L"Live preview");
        ui.Add(preview_title_, L"A thin, native layer");
        ui.Add(preview_body_, L"mwfl asks DWM for modern window composition while keeping normal Win32 controls and explicit HWND ownership.");
        ui.Add(system_, L"");
        ui.Add(note_, L"DWM options are best-effort. High Contrast automatically falls back to system-safe defaults.");
        ui.Add(status_, L"System + Mica");

        mwfl::Must(mwfl::AddItems(mode_, {L"Follow system", L"Light title bar", L"Dark title bar"}), "populate color modes");
        mwfl::Must(mwfl::AddItems(backdrop_, {L"None", L"Mica", L"Acrylic", L"Tabbed"}), "populate backdrops");
        mode_.SetSelection(0);
        backdrop_.SetSelection(1);
        rounded_.SetChecked(true);
        UpdateSystemSummary();
        mwfl::SetAccessibleName(mode_.GetHwnd(), L"Window color mode");
        mwfl::SetAccessibleName(backdrop_.GetHwnd(), L"Window backdrop material");
        mwfl::SetDialogDefaultButton(GetHwnd(), static_cast<UINT>(apply_.GetId().value));
        ApplySelection();
        ApplyFont(GetDpiContext().GetDpi());

        SetLayout(mwfl::Column().Margin(26.0_dip).Gap(11.0_dip)
            .Add(title_, mwfl::Fixed(34.0_dip)).Add(subtitle_, mwfl::Fixed(26.0_dip))
            .Add(mwfl::Row().Gap(16.0_dip)
                .Add(mwfl::Column().Gap(9.0_dip).Add(settings_, mwfl::Fixed(28.0_dip))
                    .Add(mode_label_, mwfl::Auto()).Add(mode_, mwfl::Fixed(38.0_dip))
                    .Add(backdrop_label_, mwfl::Auto()).Add(backdrop_, mwfl::Fixed(38.0_dip))
                    .Add(rounded_, mwfl::Auto()).Add(apply_, mwfl::Fixed(42.0_dip))
                    .Add(system_, mwfl::Auto()).Add(note_, mwfl::Stretch()), mwfl::Fixed(360.0_dip))
                .Add(mwfl::Column().Gap(12.0_dip).Add(preview_, mwfl::Fixed(28.0_dip))
                    .Add(preview_title_, mwfl::Auto()).Add(preview_body_, mwfl::Stretch()).Add(status_, mwfl::Fixed(40.0_dip)), mwfl::Stretch()),
                mwfl::Stretch()));
    }

    mwfl::EventResult OnCommand(const mwfl::CommandEvent& event) override {
        if (event.IsClicked(apply_) || event.Is(mode_, CBN_SELCHANGE) || event.Is(backdrop_, CBN_SELCHANGE) || event.IsClicked(rounded_)) {
            ApplySelection();
            return mwfl::EventResult::Handled();
        }
        return mwfl::EventResult::Propagate();
    }
    mwfl::EventResult OnDpiChanged(const mwfl::DpiChangedEvent& event) override { ApplyFont(event.dpi_x); return mwfl::EventResult::Propagate(); }

private:
    void ApplySelection() {
        static constexpr std::array modes{mwfl::ColorMode::system, mwfl::ColorMode::light, mwfl::ColorMode::dark};
        static constexpr std::array backdrops{mwfl::Backdrop::none, mwfl::Backdrop::mica, mwfl::Backdrop::acrylic, mwfl::Backdrop::tabbed};
        const int mode = (std::max)(0, mode_.GetSelection());
        const int backdrop = (std::max)(0, backdrop_.GetSelection());
        const mwfl::AppearanceOptions options{modes[static_cast<size_t>(mode)], backdrops[static_cast<size_t>(backdrop)], rounded_.IsChecked()};
        const bool applied = mwfl::ApplyWindowAppearance(GetHwnd(), options);
        static constexpr const wchar_t* mode_names[]{L"System", L"Light", L"Dark"};
        static constexpr const wchar_t* backdrop_names[]{L"None", L"Mica", L"Acrylic", L"Tabbed"};
        status_.SetText(std::wstring(applied ? L"Applied: " : L"Requested: ") + mode_names[mode] + L" + " + backdrop_names[backdrop]);
        UpdateSystemSummary();
    }
    void UpdateSystemSummary() {
        system_.SetText(std::wstring(L"System preference: ") + (mwfl::IsSystemDarkModePreferred() ? L"dark" : L"light") +
            L"\r\nHigh Contrast: " + (mwfl::IsHighContrastEnabled() ? L"on" : L"off"));
    }
    void ApplyFont(UINT dpi) {
        if (!font_.CreateMessageFont(dpi)) return;
        for (HWND child = ::GetWindow(GetHwnd(), GW_CHILD); child; child = ::GetWindow(child, GW_HWNDNEXT))
            ::SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(font_.GetHandle()), TRUE);
    }
    mwfl::Label title_, subtitle_, mode_label_, backdrop_label_, preview_title_, preview_body_, system_, note_, status_;
    mwfl::GroupBox settings_, preview_;
    mwfl::ComboBox mode_, backdrop_;
    mwfl::CheckBox rounded_;
    mwfl::Button apply_;
    mwfl::UiFont font_;
};

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    return mwfl::RunApplication<AppearanceWindow>(instance, show, {.title = L"Appearance lab", .initial_bounds = {{}, {1040.0_dip, 680.0_dip}}, .use_default_bounds = false});
}
