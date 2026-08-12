#include <mwfl/mwfl.h>

#include "settings_model.h"

#include <oleacc.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using mwfl::operator""_dip;

namespace {

constexpr UINT kRunSelfTest = WM_APP + 0x130;
bool g_self_test = false;
std::wstring g_settings_key = L"Software\\mwfl\\Examples\\Settings\\1";

bool HasAccessibleName(HWND window, std::wstring_view expected) {
    IAccessible* accessible = nullptr;
    if (FAILED(::AccessibleObjectFromWindow(window, static_cast<DWORD>(OBJID_CLIENT),
                                            IID_IAccessible,
                                            reinterpret_cast<void**>(&accessible))))
        return false;
    VARIANT child{};
    child.vt = VT_I4;
    child.lVal = CHILDID_SELF;
    BSTR name = nullptr;
    const bool matches = SUCCEEDED(accessible->get_accName(child, &name)) && name != nullptr &&
                         std::wstring_view{name, ::SysStringLen(name)} == expected;
    if (name != nullptr) ::SysFreeString(name);
    accessible->Release();
    return matches;
}

class PropertySheetWindow final : public mwfl::WindowBase {
public:
    void BuildUI() override {
        SetTitle(L"mwfl Settings");
        mwfl::ControlHost ui{*this};
        ui.Add(summary_, L"");
        ui.Add(open_, L"Open settings");
        SetLayout(mwfl::Column()
                      .Margin(24.0_dip)
                      .Gap(12.0_dip)
                      .Add(summary_, mwfl::Fixed(40.0_dip))
                      .Add(open_, mwfl::Fixed(36.0_dip)));
        mwfl::SetAccessibleName(open_.GetHwnd(), L"Open application settings");
        const auto loaded = settings_example::LoadSettings(HKEY_CURRENT_USER, g_settings_key);
        if (loaded.Succeeded()) {
            committed_ = *loaded.value;
        } else if (loaded.status != settings_example::StoreStatus::not_found) {
            mwfl::ShowTaskDialog(GetHwnd(), L"mwfl Settings", L"Saved settings were not loaded",
                                 L"The stored data was inaccessible or invalid. Defaults are in use.");
        }
        UpdateSummary();
        if (g_self_test && ::PostMessageW(GetHwnd(), kRunSelfTest, 0, 0) == FALSE)
            throw std::runtime_error("post Settings self-test message failed");
    }

    mwfl::EventResult OnCommand(const mwfl::CommandEvent& event) override {
        if (!event.IsClicked(open_)) return mwfl::EventResult::Propagate();
        OpenSettings();
        return mwfl::EventResult::Handled();
    }

    mwfl::EventResult OnClose() override {
        sheet_.Close();
        return mwfl::EventResult::Propagate();
    }

    mwfl::EventResult OnMessage(const mwfl::WindowMessage& event) override {
        if (event.id == WM_THEMECHANGED || event.id == WM_SETTINGCHANGE) {
            ::RedrawWindow(GetHwnd(), nullptr, nullptr,
                           RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
            return mwfl::EventResult::Handled();
        }
        if (event.id != kRunSelfTest) return mwfl::EventResult::Propagate();
        try {
            RunSelfTest();
        } catch (...) {
            ::RegDeleteTreeW(HKEY_CURRENT_USER, g_settings_key.c_str());
            ::PostQuitMessage(1);
        }
        return mwfl::EventResult::Handled();
    }

private:
    void OpenSettings() {
        if (sheet_.IsWindow()) {
            ::SetForegroundWindow(sheet_.GetHwnd());
            return;
        }
        pages_.clear();
        pages_.reserve(2);
        pages_.emplace_back(mwfl::PropertyPageOptions{
            {1},
            L"Profile",
            {
                .initialize =
                    [this](HWND page) {
                        mwfl::ControlHost ui{page};
                        ui.Add(name_label_, {101}, L"Display name", {});
                        ui.Add(name_, {102}, committed_.display_name, {});
                        mwfl::SetAccessibleName(name_.GetHwnd(), L"Display name");
                        return pages_[0].SetLayout(mwfl::Column()
                                                       .Margin(16.0_dip)
                                                       .Gap(8.0_dip)
                                                       .Add(name_label_, mwfl::Fixed(24.0_dip))
                                                       .Add(name_, mwfl::Fixed(34.0_dip)));
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
                            return mwfl::PropertyPageValidation::valid;
                        mwfl::ShowTaskDialog(page, L"Check the profile",
                                             L"Display name is required",
                                             L"Enter a non-blank display name of at most 128 characters.");
                        name_.Focus();
                        return mwfl::PropertyPageValidation::invalid;
                    },
                .apply =
                    [this](HWND page) {
                        auto candidate = committed_;
                        candidate.display_name = name_.GetText();
                        return Commit(page, std::move(candidate));
                    },
                .reset = [this](HWND) { name_.SetText(committed_.display_name); },
            }});
        pages_.emplace_back(mwfl::PropertyPageOptions{
            {2},
            L"Notifications",
            {
                .initialize =
                    [this](HWND page) {
                        mwfl::ControlHost ui{page};
                        ui.Add(notifications_, {201}, L"Show activity notifications", {});
                        notifications_.SetChecked(committed_.show_notifications);
                        return pages_[1].SetLayout(mwfl::Column().Margin(16.0_dip).Add(
                            notifications_, mwfl::Fixed(32.0_dip)));
                    },
                .command =
                    [this](HWND, WORD id, WORD notification) {
                        if (id != 201 || notification != BN_CLICKED) return false;
                        pages_[1].SetDirty();
                        return true;
                    },
                .validate = [](HWND) { return mwfl::PropertyPageValidation::valid; },
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
            mwfl::ShowTaskDialog(GetHwnd(), L"mwfl", L"Settings could not be opened",
                                 L"The native property sheet returned an error.");
        }
    }

    bool Commit(HWND page, settings_example::Settings candidate) {
        const auto saved =
            settings_example::SaveSettings(HKEY_CURRENT_USER, g_settings_key, candidate);
        if (!saved.Succeeded()) {
            mwfl::ShowTaskDialog(
                page, L"mwfl Settings", L"Settings were not saved",
                L"Check that the current account can write its user settings, then try again.");
            return false;
        }
        committed_ = std::move(candidate);
        UpdateSummary();
        return true;
    }

    void RunSelfTest() {
        if (!HasAccessibleName(open_.GetHwnd(), L"Open application settings"))
            throw std::runtime_error("Settings launcher accessible name mismatch");
        OpenSettings();
        if (!sheet_.IsWindow() || pages_.size() != 2 || pages_[0].GetHwnd() == nullptr)
            throw std::runtime_error("Settings sheet did not create its profile page");
        if (!HasAccessibleName(name_.GetHwnd(), L"Display name"))
            throw std::runtime_error("Settings page accessible name mismatch");

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

        ::SendMessageW(GetHwnd(), WM_THEMECHANGED, 0, 0);
        ::SendMessageW(GetHwnd(), WM_SETTINGCHANGE, 0, 0);
        if (!open_.IsWindow() || !sheet_.IsWindow())
            throw std::runtime_error("Settings appearance refresh destroyed controls");

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
    mwfl::Label summary_, name_label_;
    mwfl::Button open_;
    mwfl::TextBox name_;
    mwfl::CheckBox notifications_;
    std::vector<mwfl::PropertyPage> pages_;
    mwfl::PropertySheetDialog sheet_;
};

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    g_self_test = wcsstr(::GetCommandLineW(), L"--self-test") != nullptr;
    if (g_self_test) {
        g_settings_key = L"Software\\mwfl\\Tests\\SettingsApplicationGui-" +
                         std::to_wstring(::GetCurrentProcessId());
        ::RegDeleteTreeW(HKEY_CURRENT_USER, g_settings_key.c_str());
    }
    return mwfl::RunApplication<PropertySheetWindow>(
        instance, show,
        {.title = L"mwfl Settings",
         .initial_bounds = {{0.0_dip, 0.0_dip}, {520.0_dip, 220.0_dip}},
         .use_default_bounds = false});
}
