#include <mwtl/mwtl.h>

#include <vector>

using mwtl::operator""_dip;

namespace {

class PropertySheetWindow final : public mwtl::WindowBase {
public:
    void BuildUI() override {
        SetTitle(L"Property sheet host");
        mwtl::ControlHost ui{*this};
        ui.Add(summary_, L"Edit settings across two modeless pages.");
        ui.Add(open_, L"Open settings");
        SetLayout(mwtl::Column()
                      .Margin(24.0_dip)
                      .Gap(12.0_dip)
                      .Add(summary_, mwtl::Fixed(40.0_dip))
                      .Add(open_, mwtl::Fixed(36.0_dip)));
        mwtl::SetAccessibleName(open_.GetHwnd(), L"Open application settings");
    }

    mwtl::EventResult OnCommand(const mwtl::CommandEvent& event) override {
        if (!event.IsClicked(open_)) return mwtl::EventResult::Propagate();
        OpenSettings();
        return mwtl::EventResult::Handled();
    }

private:
    void OpenSettings() {
        if (sheet_.IsWindow()) {
            ::SetForegroundWindow(sheet_.GetHwnd());
            return;
        }
        pages_.clear();
        pages_.reserve(2);
        pages_.emplace_back(mwtl::PropertyPageOptions{
            {1},
            L"Profile",
            {
                .initialize =
                    [this](HWND page) {
                        mwtl::ControlHost ui{page};
                        ui.Add(name_label_, {101}, L"Display name", {});
                        ui.Add(name_, {102}, committed_name_, {});
                        mwtl::SetAccessibleName(name_.GetHwnd(), L"Display name");
                        return pages_[0].SetLayout(mwtl::Column()
                                                       .Margin(16.0_dip)
                                                       .Gap(8.0_dip)
                                                       .Add(name_label_, mwtl::Fixed(24.0_dip))
                                                       .Add(name_, mwtl::Fixed(34.0_dip)));
                    },
                .command =
                    [this](HWND, WORD id, WORD notification) {
                        if (id != 102 || notification != EN_CHANGE) return false;
                        pages_[0].SetDirty();
                        return true;
                    },
                .validate =
                    [this](HWND page) {
                        if (!name_.GetText().empty()) return mwtl::PropertyPageValidation::valid;
                        mwtl::ShowTaskDialog(page, L"Check the profile",
                                             L"Display name is required",
                                             L"Enter a name before applying these settings.");
                        name_.Focus();
                        return mwtl::PropertyPageValidation::invalid;
                    },
                .apply =
                    [this](HWND) {
                        committed_name_ = name_.GetText();
                        UpdateSummary();
                        return true;
                    },
                .reset = [this](HWND) { name_.SetText(committed_name_); },
            }});
        pages_.emplace_back(mwtl::PropertyPageOptions{
            {2},
            L"Notifications",
            {
                .initialize =
                    [this](HWND page) {
                        mwtl::ControlHost ui{page};
                        ui.Add(notifications_, {201}, L"Show activity notifications", {});
                        notifications_.SetChecked(committed_notifications_);
                        return pages_[1].SetLayout(mwtl::Column().Margin(16.0_dip).Add(
                            notifications_, mwtl::Fixed(32.0_dip)));
                    },
                .command =
                    [this](HWND, WORD id, WORD notification) {
                        if (id != 201 || notification != BN_CLICKED) return false;
                        pages_[1].SetDirty();
                        return true;
                    },
                .validate = [](HWND) { return mwtl::PropertyPageValidation::valid; },
                .apply =
                    [this](HWND) {
                        committed_notifications_ = notifications_.IsChecked();
                        UpdateSummary();
                        return true;
                    },
                .reset = [this](HWND) { notifications_.SetChecked(committed_notifications_); },
            }});
        if (!sheet_.CreateModeless({.owner = GetHwnd(), .title = L"Application settings"},
                                   pages_)) {
            mwtl::ShowTaskDialog(GetHwnd(), L"mwtl", L"Settings could not be opened",
                                 L"The native property sheet returned an error.");
        }
    }

    void UpdateSummary() {
        summary_.SetText(committed_name_ + (committed_notifications_ ? L" — notifications on"
                                                                     : L" — notifications off"));
    }

    std::wstring committed_name_ = L"Ada Lovelace";
    bool committed_notifications_ = true;
    mwtl::Label summary_, name_label_;
    mwtl::Button open_;
    mwtl::TextBox name_;
    mwtl::CheckBox notifications_;
    std::vector<mwtl::PropertyPage> pages_;
    mwtl::PropertySheetDialog sheet_;
};

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    return mwtl::RunApplication<PropertySheetWindow>(
        instance, show,
        {.title = L"Property sheet host",
         .initial_bounds = {{0.0_dip, 0.0_dip}, {520.0_dip, 220.0_dip}},
         .use_default_bounds = false});
}
