#include <mwtl/mwtl.h>

#include "settings_model.h"

#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using mwtl::operator""_dip;

namespace {

constexpr UINT kRunSelfTest = WM_APP + 0x130;
bool g_self_test = false;
std::wstring g_settings_key = L"Software\\mwtl\\Examples\\Settings\\1";

class PropertySheetWindow final : public mwtl::WindowBase {
public:
    void BuildUI() override {
        SetTitle(L"mwtl Settings");
        mwtl::ControlHost ui{*this};
        ui.Add(summary_, L"");
        ui.Add(open_, L"Open settings");
        SetLayout(mwtl::Column()
                      .Margin(24.0_dip)
                      .Gap(12.0_dip)
                      .Add(summary_, mwtl::Fixed(40.0_dip))
                      .Add(open_, mwtl::Fixed(36.0_dip)));
        mwtl::SetAccessibleName(open_.GetHwnd(), L"Open application settings");
        const auto loaded = settings_example::LoadSettings(HKEY_CURRENT_USER, g_settings_key);
        if (loaded.Succeeded()) {
            committed_ = *loaded.value;
        } else if (loaded.status != settings_example::StoreStatus::not_found) {
            mwtl::ShowTaskDialog(GetHwnd(), L"mwtl Settings", L"Saved settings were not loaded",
                                 L"The stored data was inaccessible or invalid. Defaults are in use.");
        }
        UpdateSummary();
        if (g_self_test && ::PostMessageW(GetHwnd(), kRunSelfTest, 0, 0) == FALSE)
            throw std::runtime_error("post Settings self-test message failed");
    }

    mwtl::EventResult OnCommand(const mwtl::CommandEvent& event) override {
        if (!event.IsClicked(open_)) return mwtl::EventResult::Propagate();
        OpenSettings();
        return mwtl::EventResult::Handled();
    }

    mwtl::EventResult OnClose() override {
        sheet_.Close();
        return mwtl::EventResult::Propagate();
    }

    mwtl::EventResult OnMessage(const mwtl::WindowMessage& event) override {
        if (event.id != kRunSelfTest) return mwtl::EventResult::Propagate();
        try {
            RunSelfTest();
        } catch (...) {
            ::RegDeleteTreeW(HKEY_CURRENT_USER, g_settings_key.c_str());
            ::PostQuitMessage(1);
        }
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
                        ui.Add(name_, {102}, committed_.display_name, {});
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
                        auto candidate = committed_;
                        candidate.display_name = name_.GetText();
                        if (settings_example::IsValid(candidate))
                            return mwtl::PropertyPageValidation::valid;
                        mwtl::ShowTaskDialog(page, L"Check the profile",
                                             L"Display name is required",
                                             L"Enter a non-blank display name of at most 128 characters.");
                        name_.Focus();
                        return mwtl::PropertyPageValidation::invalid;
                    },
                .apply =
                    [this](HWND page) {
                        auto candidate = committed_;
                        candidate.display_name = name_.GetText();
                        return Commit(page, std::move(candidate));
                    },
                .reset = [this](HWND) { name_.SetText(committed_.display_name); },
            }});
        pages_.emplace_back(mwtl::PropertyPageOptions{
            {2},
            L"Notifications",
            {
                .initialize =
                    [this](HWND page) {
                        mwtl::ControlHost ui{page};
                        ui.Add(notifications_, {201}, L"Show activity notifications", {});
                        notifications_.SetChecked(committed_.show_notifications);
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
                    [this](HWND page) {
                        auto candidate = committed_;
                        candidate.show_notifications = notifications_.IsChecked();
                        return Commit(page, std::move(candidate));
                    },
                .reset = [this](HWND) {
                    notifications_.SetChecked(committed_.show_notifications);
                },
            }});
        if (!sheet_.CreateModeless({.owner = GetHwnd(), .title = L"Application settings"},
                                   pages_)) {
            mwtl::ShowTaskDialog(GetHwnd(), L"mwtl", L"Settings could not be opened",
                                 L"The native property sheet returned an error.");
        }
    }

    bool Commit(HWND page, settings_example::Settings candidate) {
        const auto saved =
            settings_example::SaveSettings(HKEY_CURRENT_USER, g_settings_key, candidate);
        if (!saved.Succeeded()) {
            mwtl::ShowTaskDialog(
                page, L"mwtl Settings", L"Settings were not saved",
                L"Check that the current account can write its user settings, then try again.");
            return false;
        }
        committed_ = std::move(candidate);
        UpdateSummary();
        return true;
    }

    void RunSelfTest() {
        OpenSettings();
        if (!sheet_.IsWindow() || pages_.size() != 2 || pages_[0].GetHwnd() == nullptr)
            throw std::runtime_error("Settings sheet did not create its profile page");

        name_.SetText(L"Grace Hopper");
        ::SendMessageW(pages_[0].GetHwnd(), WM_COMMAND, MAKEWPARAM(102, EN_CHANGE),
                       reinterpret_cast<LPARAM>(name_.GetHwnd()));
        if (!pages_[0].IsDirty()) throw std::runtime_error("profile edit did not become dirty");
        ::SendMessageW(sheet_.GetHwnd(), PSM_PRESSBUTTON, PSBTN_APPLYNOW, 0);
        const auto profile = settings_example::LoadSettings(HKEY_CURRENT_USER, g_settings_key);
        if (!profile.Succeeded() || profile.value->display_name != L"Grace Hopper" ||
            pages_[0].IsDirty())
            throw std::runtime_error("profile Apply did not persist committed state");

        if (::SendMessageW(sheet_.GetHwnd(), PSM_SETCURSEL, 1, 0) == FALSE ||
            pages_[1].GetHwnd() == nullptr)
            throw std::runtime_error("notifications page did not activate");
        notifications_.SetChecked(false);
        ::SendMessageW(pages_[1].GetHwnd(), WM_COMMAND, MAKEWPARAM(201, BN_CLICKED),
                       reinterpret_cast<LPARAM>(notifications_.GetHwnd()));
        ::SendMessageW(sheet_.GetHwnd(), PSM_PRESSBUTTON, PSBTN_APPLYNOW, 0);
        const auto notifications =
            settings_example::LoadSettings(HKEY_CURRENT_USER, g_settings_key);
        if (!notifications.Succeeded() || notifications.value->show_notifications ||
            summary_.GetText().find(L"notifications off") == std::wstring::npos)
            throw std::runtime_error("notification Apply did not update persisted presentation");

        ::SendMessageW(sheet_.GetHwnd(), PSM_PRESSBUTTON, PSBTN_CANCEL, 0);
        if (sheet_.IsWindow()) throw std::runtime_error("Settings Cancel did not close the sheet");
        ::RegDeleteTreeW(HKEY_CURRENT_USER, g_settings_key.c_str());
        if (::PostMessageW(GetHwnd(), WM_CLOSE, 0, 0) == FALSE)
            throw std::runtime_error("post Settings self-test close failed");
    }

    void UpdateSummary() {
        summary_.SetText(committed_.display_name +
                         (committed_.show_notifications ? L" — notifications on"
                                                        : L" — notifications off"));
    }

    settings_example::Settings committed_;
    mwtl::Label summary_, name_label_;
    mwtl::Button open_;
    mwtl::TextBox name_;
    mwtl::CheckBox notifications_;
    std::vector<mwtl::PropertyPage> pages_;
    mwtl::PropertySheetDialog sheet_;
};

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    g_self_test = wcsstr(::GetCommandLineW(), L"--self-test") != nullptr;
    if (g_self_test) {
        g_settings_key = L"Software\\mwtl\\Tests\\SettingsApplicationGui-" +
                         std::to_wstring(::GetCurrentProcessId());
        ::RegDeleteTreeW(HKEY_CURRENT_USER, g_settings_key.c_str());
    }
    return mwtl::RunApplication<PropertySheetWindow>(
        instance, show,
        {.title = L"mwtl Settings",
         .initial_bounds = {{0.0_dip, 0.0_dip}, {520.0_dip, 220.0_dip}},
         .use_default_bounds = false});
}
