#include <mwfl/mwfl.h>

#include <chrono>
#include <cstdlib>
#include <stdexcept>
#include <string>

using namespace std::chrono_literals;
using mwfl::operator""_dip;

class ControlsWindow final : public mwfl::WindowBase {
public:
    void BuildUI() override {
        SetTitle(L"Modern native controls");
        mwfl::ControlHost ui{*this};
        ui.Add(heading_, L"Profile and preferences");
        ui.Add(subtitle_, L"A product-style form built entirely from native HWND controls.");
        ui.Add(profile_group_, L"Profile");
        ui.Add(name_label_, L"Your name");
        ui.Add(name_, L"mwfl developer");
        ui.Add(greet_, L"Say hello");
        ui.Add(enabled_, L"Keep the native button enabled");
        ui.Add(accent_);
        ui.Add(progress_);
        ui.Add(status_, L"Ready — changes are reflected immediately");
        ui.Add(choices_, L"Theme");
        ui.Add(sky_, L"Sky blue", mwfl::RadioButtonOptions{
            .style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_GROUP |
                     BS_AUTORADIOBUTTON});
        ui.Add(cosmos_, L"Cosmic violet");
        ui.Add(items_);
        ui.Add(volume_);
        mwfl::Must(
            heartbeat_.Start(*this, kHeartbeat, 1s), "start heartbeat");
        enabled_.SetChecked(true);
        mwfl::Must(mwfl::AddItems(
            accent_, {L"Sunrise orange", L"Geometry blue", L"Optimistic green"}),
            "populate accent choices");
        accent_.SetSelection(0);
        progress_.SetRange(0, 100);
        progress_.SetValue(18);
        sky_.SetChecked(true);
        mwfl::Must(mwfl::AddItems(
            items_, {L"Label + text input", L"Buttons and choices", L"Lists and progress"}),
            "populate control topics");
        items_.SetSelection(0);
        volume_.SetRange(0, 100);
        volume_.SetValue(65);
        name_.SelectAll();
        name_.Focus();
        mwfl::Must(name_.SetCueBanner(L"Enter a display name"),
                   "set profile name cue banner");
        SetAppearance({mwfl::ColorMode::system, mwfl::Backdrop::mica});

        SetLayout(
            mwfl::Row()
                .Margin(32.0_dip)
                .Gap(28.0_dip)
                .Add(
                    mwfl::Column()
                        .Gap(12.0_dip)
                        .Add(heading_, mwfl::Fixed(34.0_dip))
                        .Add(subtitle_, mwfl::Fixed(26.0_dip))
                        .Add(mwfl::Overlay().Add(profile_group_).Add(
                            mwfl::Column().Margin({20.0_dip, 34.0_dip, 20.0_dip, 18.0_dip})
                                .Gap(10.0_dip)
                                .Add(name_label_, mwfl::Fixed(22.0_dip))
                                .Add(mwfl::Row().Gap(16.0_dip)
                                    .Add(name_, mwfl::Stretch(1.0f, 180.0_dip))
                                    .Add(greet_, mwfl::Fixed(132.0_dip)), mwfl::Fixed(34.0_dip))
                                .Add(mwfl::Row().Gap(16.0_dip)
                                    .Add(enabled_, mwfl::Stretch())
                                    .Add(accent_, mwfl::Fixed(220.0_dip), {
                                        .native_size = mwfl::SizeDip{0.0_dip, 180.0_dip},
                                    }), mwfl::Fixed(34.0_dip))), mwfl::Fixed(150.0_dip))
                        .Add(progress_, mwfl::Fixed(22.0_dip))
                        .Add(status_, mwfl::Stretch(1.0f, 50.0_dip)),
                    mwfl::Stretch(1.0f, 540.0_dip))
                .Add(
                    mwfl::Column()
                        .Gap(18.0_dip)
                        .Add(
                            mwfl::Overlay()
                                .Add(choices_)
                                .Add(
                                    mwfl::Column()
                                        .Margin({20.0_dip, 32.0_dip, 20.0_dip, 18.0_dip})
                                        .Gap(6.0_dip)
                                        .Add(sky_, mwfl::Fixed(28.0_dip))
                                        .Add(cosmos_, mwfl::Fixed(28.0_dip))),
                            mwfl::Fixed(154.0_dip))
                        .Add(items_, mwfl::Stretch(1.0f, 100.0_dip))
                        .Add(volume_, mwfl::Fixed(42.0_dip)),
                    mwfl::Fixed(300.0_dip)));
    }

    mwfl::EventResult OnCommand(const mwfl::CommandEvent& event) override {
        if (event.IsClicked(greet_)) {
            status_.SetText(
                L"Hello, " + name_.GetText() +
                L"! Button notification received.");
        } else if (event.IsClicked(enabled_)) {
            greet_.SetEnabled(enabled_.IsChecked());
        } else if (event.Is(accent_, CBN_SELCHANGE)) {
            status_.SetText(
                L"ComboBox selection changed; still a native notification.");
        } else if (event.IsClicked(sky_) || event.IsClicked(cosmos_)) {
            const bool sky = event.IsClicked(sky_);
            ::PostMessageW(sky_.GetHwnd(), BM_SETCHECK,
                           sky ? BST_CHECKED : BST_UNCHECKED, 0);
            ::PostMessageW(cosmos_.GetHwnd(), BM_SETCHECK,
                           sky ? BST_UNCHECKED : BST_CHECKED, 0);
            status_.SetText(L"RadioButton choice changed.");
        } else if (event.Is(items_, LBN_SELCHANGE)) {
            status_.SetText(L"ListBox selection changed.");
        } else {
            return mwfl::EventResult::Propagate();
        }
        return mwfl::EventResult::Handled();
    }

    mwfl::EventResult OnTimer(mwfl::TimerId id) override {
        if (id == kHeartbeat) {
            ++ticks_;
            progress_.SetValue(static_cast<int>((ticks_ * 9) % 101));
            SetTitle(L"Modern native controls — heartbeat " +
                     std::to_wstring(ticks_));
            return mwfl::EventResult::Handled();
        }
        return mwfl::EventResult::Propagate();
    }

    mwfl::EventResult OnMessage(const mwfl::WindowMessage& message) override {
        if (message.id == WM_HSCROLL &&
            reinterpret_cast<HWND>(message.lparam) == volume_.GetHwnd()) {
            progress_.SetValue(volume_.GetValue());
            return mwfl::EventResult::Handled();
        }
        return mwfl::EventResult::Propagate();
    }

private:
    static constexpr mwfl::TimerId kHeartbeat{1};

    mwfl::Label heading_;
    mwfl::Label subtitle_;
    mwfl::GroupBox profile_group_;
    mwfl::Label name_label_;
    mwfl::TextBox name_;
    mwfl::Button greet_;
    mwfl::CheckBox enabled_;
    mwfl::ComboBox accent_;
    mwfl::ProgressBar progress_;
    mwfl::Label status_;
    mwfl::GroupBox choices_;
    mwfl::RadioButton sky_;
    mwfl::RadioButton cosmos_;
    mwfl::ListBox items_;
    mwfl::Slider volume_;
    mwfl::UiTimer heartbeat_;
    unsigned ticks_ = 0;
};

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
    return mwfl::RunApplication<ControlsWindow>(
        instance,
        show_command,
        {
            .title = L"Modern native controls",
            .initial_bounds = {{0.0_dip, 0.0_dip}, {960.0_dip, 500.0_dip}},
            .use_default_bounds = false,
        });
}
