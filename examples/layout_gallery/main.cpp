#include <mwfl/mwfl.h>

#include <string>

using mwfl::operator""_dip;

namespace {

class LayoutGalleryWindow final : public mwfl::WindowBase {
public:
    void BuildUI() override {
        SetTitle(L"Responsive layout gallery");
        mwfl::ControlHost ui{*this};
        ui.Add(title_, L"Responsive layout gallery");
        ui.Add(subtitle_, L"Resize the window and switch density to see nested Row, Column, Overlay, Auto, Fixed, and Stretch behavior.");
        ui.Add(compact_, L"Compact");
        ui.Add(comfortable_, L"Comfortable");
        ui.Add(long_text_, L"Toggle long content");
        ui.Add(profile_, L"Profile card - Overlay");
        ui.Add(avatar_, L"AL");
        ui.Add(name_, L"Ada Lovelace");
        ui.Add(role_, L"Computing pioneer");
        ui.Add(active_, L"Active");
        ui.Add(metrics_, L"Weighted stretch row");
        ui.Add(metric_a_, L"12\r\nProjects");
        ui.Add(metric_b_, L"48\r\nReviews");
        ui.Add(metric_c_, L"99%\r\nUptime");
        ui.Add(form_, L"Auto labels + stretch inputs");
        ui.Add(email_label_, L"Email");
        ui.Add(email_, L"ada@example.com");
        ui.Add(team_label_, L"Team");
        ui.Add(team_);
        ui.Add(save_, L"Save profile");
        ui.Add(explainer_, L"Layout recipe");
        ui.Add(recipe_, L"Root Column\r\n  Toolbar: Auto\r\n  Content Row: Stretch\r\n    Cards: weighted Stretch\r\n  Footer: Fixed");
        ui.Add(status_, L"Comfortable density | resize to test");
        mwfl::Must(mwfl::AddItems(team_, {L"Research", L"Design systems", L"Developer experience"}), "populate teams");
        team_.SetSelection(2);
        comfortable_.SetChecked(true);
        mwfl::SetDialogDefaultButton(GetHwnd(), static_cast<UINT>(save_.GetId().value));
        SetAppearance({mwfl::ColorMode::system, mwfl::Backdrop::mica});
        ApplyFont(GetDpiContext().GetDpi());
        RebuildLayout();
    }

    mwfl::EventResult OnCommand(const mwfl::CommandEvent& event) override {
        if (event.IsClicked(compact_)) {
            compact_.SetChecked(true); comfortable_.SetChecked(false); compact_mode_ = true;
            status_.SetText(L"Compact density | gaps and card padding reduced");
            RebuildLayout();
            return mwfl::EventResult::Handled();
        }
        if (event.IsClicked(comfortable_)) {
            compact_.SetChecked(false); comfortable_.SetChecked(true); compact_mode_ = false;
            status_.SetText(L"Comfortable density | resize to test");
            RebuildLayout();
            return mwfl::EventResult::Handled();
        }
        if (event.IsClicked(long_text_)) {
            long_content_ = !long_content_;
            role_.SetText(long_content_ ? L"Mathematician, writer, and visionary of general-purpose computing" : L"Computing pioneer");
            email_label_.SetText(long_content_ ? L"Primary contact email address" : L"Email");
            status_.SetText(long_content_ ? L"Long content enabled | Auto measurement updated" : L"Short content restored");
            RebuildLayout();
            return mwfl::EventResult::Handled();
        }
        if (event.IsClicked(save_)) {
            status_.SetText(L"Saved | the footer remains fixed while content stretches");
            return mwfl::EventResult::Handled();
        }
        return mwfl::EventResult::Propagate();
    }

    mwfl::EventResult OnDpiChanged(const mwfl::DpiChangedEvent& event) override {
        ApplyFont(event.dpi_x); return mwfl::EventResult::Propagate();
    }

private:
    mwfl::LayoutNode ProfileCard(mwfl::Dip pad, mwfl::Dip gap) {
        return mwfl::Overlay().Add(profile_).Add(
            mwfl::Column().Margin({pad, 36.0_dip, pad, pad}).Gap(gap)
                .Add(mwfl::Row().Gap(gap)
                    .Add(avatar_, mwfl::Fixed(62.0_dip), {.alignment = mwfl::CrossAlignment::center})
                    .Add(mwfl::Column().Gap(4.0_dip).Add(name_, mwfl::Auto()).Add(role_, mwfl::Stretch()), mwfl::Stretch())
                    .Add(active_, mwfl::Fixed(74.0_dip), {.alignment = mwfl::CrossAlignment::start}), mwfl::Fixed(110.0_dip))
                .Add(mwfl::Column(), mwfl::Stretch())
                .Add(mwfl::Overlay().Add(metrics_).Add(
                    mwfl::Row().Margin({14.0_dip, 30.0_dip, 14.0_dip, 10.0_dip}).Gap(gap)
                        .Add(metric_a_, mwfl::Stretch(1.0f)).Add(metric_b_, mwfl::Stretch(1.4f)).Add(metric_c_, mwfl::Stretch(1.0f))),
                    mwfl::Fixed(96.0_dip)));
    }

    mwfl::LayoutNode FormCard(mwfl::Dip pad, mwfl::Dip gap) {
        const auto label_width = long_content_ ? 190.0_dip : 74.0_dip;
        return mwfl::Overlay().Add(form_).Add(
            mwfl::Column().Margin({pad, 36.0_dip, pad, pad}).Gap(gap)
                .Add(mwfl::Row().Gap(gap).Add(email_label_, mwfl::Fixed(label_width), {.alignment = mwfl::CrossAlignment::center}).Add(email_, mwfl::Stretch()), mwfl::Fixed(38.0_dip))
                .Add(mwfl::Row().Gap(gap).Add(team_label_, mwfl::Fixed(label_width), {.alignment = mwfl::CrossAlignment::center}).Add(team_, mwfl::Stretch()), mwfl::Fixed(38.0_dip))
                .Add(mwfl::Row().Add(mwfl::Column(), mwfl::Stretch()).Add(save_, mwfl::Fixed(140.0_dip)), mwfl::Fixed(42.0_dip))
                .Add(mwfl::Overlay().Add(explainer_).Add(mwfl::Column().Margin({14.0_dip, 30.0_dip, 14.0_dip, 12.0_dip}).Add(recipe_, mwfl::Stretch())), mwfl::Stretch()));
    }

    void RebuildLayout() {
        const auto gap = compact_mode_ ? 7.0_dip : 12.0_dip;
        const auto pad = compact_mode_ ? 14.0_dip : 22.0_dip;
        SetLayout(mwfl::Column().Margin(compact_mode_ ? 16.0_dip : 24.0_dip).Gap(gap)
            .Add(title_, mwfl::Fixed(34.0_dip)).Add(subtitle_, mwfl::Fixed(28.0_dip))
            .Add(mwfl::Row().Gap(8.0_dip).Add(compact_, mwfl::Fixed(105.0_dip)).Add(comfortable_, mwfl::Fixed(120.0_dip)).Add(long_text_, mwfl::Fixed(160.0_dip)), mwfl::Fixed(34.0_dip))
            .Add(mwfl::Row().Gap(gap).Add(ProfileCard(pad, gap), mwfl::Fixed(560.0_dip)).Add(FormCard(pad, gap), mwfl::Stretch()), mwfl::Stretch())
            .Add(status_, mwfl::Fixed(30.0_dip)));
    }
    void ApplyFont(UINT dpi) {
        if (!font_.CreateMessageFont(dpi)) return;
        for (HWND child = ::GetWindow(GetHwnd(), GW_CHILD); child; child = ::GetWindow(child, GW_HWNDNEXT))
            ::SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(font_.GetHandle()), TRUE);
    }
    bool compact_mode_{};
    bool long_content_{};
    mwfl::Label title_, subtitle_, avatar_, name_, role_, active_, metric_a_, metric_b_, metric_c_, email_label_, team_label_, recipe_, status_;
    mwfl::RadioButton compact_, comfortable_;
    mwfl::Button long_text_, save_;
    mwfl::GroupBox profile_, metrics_, form_, explainer_;
    mwfl::TextBox email_;
    mwfl::ComboBox team_;
    mwfl::UiFont font_;
};

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    return mwfl::RunApplication<LayoutGalleryWindow>(instance, show, {.title = L"Responsive layout gallery", .initial_bounds = {{}, {1180.0_dip, 760.0_dip}}, .use_default_bounds = false});
}
