#include <mwtl/file_association.h>
#include <mwtl/help.h>
#include <mwtl/mwtl.h>
#include <mwtl/settings_store.h>
#include <mwtl/shell_integration.h>

#include <array>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>

using mwtl::operator""_dip;

namespace {
constexpr wchar_t kAppId[] = L"mwtl.examples.shell-integration";
constexpr wchar_t kSettingsKey[] = L"Software\\mwtl\\Examples\\ShellIntegration";
constexpr UINT kRunSelfTest = WM_APP + 0x1D0;
constexpr mwtl::ControlId kTaskbarProgress{901};
constexpr mwtl::ControlId kTaskbarHelp{902};
bool g_self_test = false;

std::filesystem::path ExecutablePath() {
    std::wstring path(32768, L'\0');
    const DWORD length = ::GetModuleFileNameW(nullptr, path.data(),
                                               static_cast<DWORD>(path.size()));
    if (length == 0 || length >= path.size()) return {};
    path.resize(length);
    return path;
}

std::wstring Describe(mwtl::ShellResult result) {
    switch (result.status) {
        case mwtl::ShellStatus::success: return L"success";
        case mwtl::ShellStatus::unavailable: return L"unavailable";
        case mwtl::ShellStatus::wrong_apartment: return L"wrong COM apartment";
        case mwtl::ShellStatus::wrong_thread: return L"wrong UI thread";
        case mwtl::ShellStatus::invalid_argument: return L"invalid argument";
        case mwtl::ShellStatus::rejected: return L"rejected by Shell";
        case mwtl::ShellStatus::com_failure: return L"COM failure";
    }
    return L"unknown";
}

mwtl::FileAssociationSpec Association() {
    return {.extension = L".mwtldemo",
            .prog_id = L"mwtl.examples.shell-integration.document",
            .owner_id = kAppId,
            .display_name = L"mwtl Shell integration document",
            .executable = ExecutablePath(),
            .icon = ExecutablePath(),
            .verbs = {{L"open", L"Open with mwtl", {}}}};
}

class ShellIntegrationWindow final : public mwtl::WindowBase {
public:
    void BuildUI() override {
        SetTitle(L"mwtl Shell Integration");
        mwtl::ControlHost ui{*this};
        ui.Add(title_, L"Safe, per-user Windows Shell integration");
        ui.Add(summary_, L"Every persistent mutation is explicit and reversible. Shell features "
                         L"report unavailable/rejected states instead of pretending success.");
        ui.Add(register_, L"Register .mwtldemo for this user");
        ui.Add(remove_, L"Remove owned association");
        ui.Add(jump_list_, L"Install Jump List tasks");
        ui.Add(remove_jump_list_, L"Remove Jump List");
        ui.Add(recent_, L"Add executable to Recent");
        ui.Add(progress_, L"Advance taskbar progress");
        ui.Add(overlay_, L"Toggle warning overlay");
        ui.Add(save_, L"Save typed settings");
        ui.Add(clear_settings_, L"Remove owned settings");
        ui.Add(help_, L"Open online help");
        ui.Add(status_, L"Ready. No machine-wide state is used.");
        mwtl::TextBoxOptions log_options;
        log_options.style |= ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL;
        ui.Add(log_, L"Startup complete.", log_options);
        log_.SetReadOnly(true);
        SetLayout(mwtl::Column()
                      .Margin(20.0_dip).Gap(10.0_dip)
                      .Add(title_, mwtl::Fixed(34.0_dip))
                      .Add(summary_, mwtl::Fixed(44.0_dip))
                      .Add(mwtl::Row().Gap(8.0_dip)
                               .Add(register_, mwtl::Auto()).Add(remove_, mwtl::Auto()),
                           mwtl::Fixed(38.0_dip))
                      .Add(mwtl::Row().Gap(8.0_dip)
                               .Add(jump_list_, mwtl::Auto()).Add(remove_jump_list_, mwtl::Auto())
                               .Add(recent_, mwtl::Auto()), mwtl::Fixed(38.0_dip))
                      .Add(mwtl::Row().Gap(8.0_dip)
                               .Add(progress_, mwtl::Auto()).Add(overlay_, mwtl::Auto()),
                           mwtl::Fixed(38.0_dip))
                      .Add(mwtl::Row().Gap(8.0_dip)
                               .Add(save_, mwtl::Auto()).Add(clear_settings_, mwtl::Auto())
                               .Add(help_, mwtl::Auto()),
                           mwtl::Fixed(38.0_dip))
                      .Add(status_, mwtl::Fixed(30.0_dip)).Add(log_, mwtl::Stretch()));
        mwtl::Must(mwtl::SetAccessibleName(GetHwnd(), L"mwtl Shell integration reference"),
                   "name Shell integration window");
        mwtl::Must(mwtl::SetAccessibleName(log_.GetHwnd(), L"Shell operation log"),
                   "name Shell operation log");
        mwtl::ApplyWindowAppearance(GetHwnd());

        Log(L"Process identity: " + Describe(mwtl::SetProcessAppUserModelId(kAppId)));
        Log(L"Window identity: " + Describe(mwtl::SetWindowAppUserModelId(GetHwnd(), kAppId)));
        Log(L"Taskbar: " + Describe(taskbar_.Create(GetHwnd())));
        ApplyThumbnailButtons();
        LoadSettings();
        if (g_self_test && !::PostMessageW(GetHwnd(), kRunSelfTest, 0, 0))
            throw std::runtime_error("post Shell self-test failed");
    }

    mwtl::EventResult OnCommand(const mwtl::CommandEvent& event) override {
        if (event.IsClicked(register_)) {
            ReportAssociation(L"Register association", mwtl::RegisterPerUserFileAssociation(Association()));
            return mwtl::EventResult::Handled();
        }
        if (event.IsClicked(remove_)) {
            ReportAssociation(L"Remove association", mwtl::RemovePerUserFileAssociation(Association()));
            return mwtl::EventResult::Handled();
        }
        if (event.IsClicked(jump_list_)) { InstallJumpList(); return mwtl::EventResult::Handled(); }
        if (event.IsClicked(remove_jump_list_)) {
            Log(L"Remove Jump List: " + Describe(mwtl::DeleteJumpList(kAppId)));
            return mwtl::EventResult::Handled();
        }
        if (event.IsClicked(recent_)) {
            Log(L"Add Recent item: " + Describe(mwtl::AddRecentDocument(ExecutablePath())));
            return mwtl::EventResult::Handled();
        }
        if (event.IsClicked(progress_)) { AdvanceProgress(); return mwtl::EventResult::Handled(); }
        if (event.IsClicked(overlay_)) { ToggleOverlay(); return mwtl::EventResult::Handled(); }
        if (event.IsClicked(save_)) { SaveSettings(); return mwtl::EventResult::Handled(); }
        if (event.IsClicked(clear_settings_)) { ClearSettings(); return mwtl::EventResult::Handled(); }
        if (event.IsClicked(help_) || event.id == kTaskbarHelp) {
            OpenHelp(); return mwtl::EventResult::Handled();
        }
        if (event.id == kTaskbarProgress) {
            AdvanceProgress(); return mwtl::EventResult::Handled();
        }
        return mwtl::EventResult::Propagate();
    }

    mwtl::EventResult OnKeyDown(const mwtl::KeyEvent& event) override {
        if (event.virtual_key == 'P' && (::GetKeyState(VK_CONTROL) & 0x8000)) {
            AdvanceProgress(); return mwtl::EventResult::Handled();
        }
        if (event.virtual_key == 'J' && (::GetKeyState(VK_CONTROL) & 0x8000)) {
            InstallJumpList(); return mwtl::EventResult::Handled();
        }
        return mwtl::EventResult::Propagate();
    }

    mwtl::EventResult OnMessage(const mwtl::WindowMessage& event) override {
        if (event.id == mwtl::TaskbarWindowIntegration::GetTaskbarCreatedMessage()) {
            Log(L"Explorer restarted; taskbar state recreated: " + Describe(taskbar_.Recreate()));
            taskbar_.Apply(progress_model_);
            ApplyThumbnailButtons();
            return mwtl::EventResult::Handled();
        }
        if (event.id == kRunSelfTest) {
            RunSelfTest(); return mwtl::EventResult::Handled();
        }
        if (event.id == WM_THEMECHANGED || event.id == WM_SYSCOLORCHANGE ||
            event.id == WM_SETTINGCHANGE) {
            mwtl::ApplyWindowAppearance(GetHwnd());
            return mwtl::EventResult::Handled();
        }
        return mwtl::EventResult::Propagate();
    }

    mwtl::EventResult OnClose() override {
        taskbar_.Clear();
        return mwtl::EventResult::Propagate();
    }

private:
    void Log(std::wstring text) {
        status_.SetText(text);
        log_.SetText(log_.GetText() + L"\r\n" + text);
    }

    void ReportAssociation(std::wstring_view action, mwtl::FileAssociationResult result) {
        Log(std::wstring(action) + (result ? L": success" : L": not changed (status " +
            std::to_wstring(static_cast<int>(result.status)) + L", error " +
            std::to_wstring(result.error) + L")"));
    }

    void InstallJumpList() {
        mwtl::JumpListCommitOptions options;
        options.app_id = kAppId;
        options.executable = ExecutablePath();
        options.tasks = {{L"progress", L"Open progress demo", L"--progress", {}, 0},
                         {L"settings", L"Open settings", L"--settings", {}, 0}};
        Log(L"Install Jump List: " + Describe(mwtl::CommitJumpList(options)));
    }

    void AdvanceProgress() {
        progress_value_ = (progress_value_ + 20) % 120;
        if (progress_value_ > 100) {
            progress_model_.Reset();
            progress_value_ = 0;
        } else {
            progress_model_.SetValue(progress_value_, 100);
            progress_model_.SetState(mwtl::TaskbarProgressState::normal);
        }
        Log(L"Taskbar progress: " + Describe(taskbar_.Apply(progress_model_)));
    }

    void ToggleOverlay() {
        overlay_visible_ = !overlay_visible_;
        HICON icon = overlay_visible_ ? ::LoadIconW(nullptr, IDI_WARNING) : nullptr;
        Log(L"Taskbar overlay: " + Describe(taskbar_.SetOverlayIcon(
            icon, overlay_visible_ ? L"Shell integration warning" : L"")));
    }

    void ApplyThumbnailButtons() {
        const std::array buttons{
            mwtl::TaskbarThumbnailButton{kTaskbarProgress.value, nullptr,
                                          L"Advance progress", THBF_ENABLED},
            mwtl::TaskbarThumbnailButton{kTaskbarHelp.value, nullptr,
                                          L"Open help", THBF_ENABLED}};
        Log(L"Taskbar thumbnail commands: " + Describe(taskbar_.SetThumbnailButtons(buttons)));
    }

    void OpenHelp() {
        const mwtl::HelpRequest request{
            mwtl::HelpTargetKind::https_uri, {}, L"https://github.com/everettjf/mwtl", L"#readme"};
        const auto result = mwtl::LaunchHelp(GetHwnd(), request);
        Log(result ? L"Help launched." : L"Help launch status: " +
            std::to_wstring(static_cast<int>(result.status)) + L", error " +
            std::to_wstring(result.error));
    }

    void SaveSettings() {
        const std::array values{mwtl::SettingValue{L"ProgressStep", std::uint32_t{20}},
                                mwtl::SettingValue{L"AppIdentity", std::wstring{kAppId}}};
        const auto result = settings_.Save(values);
        Log(result ? L"Typed settings saved." : L"Settings save failed: " +
            std::to_wstring(result.error));
    }

    void LoadSettings() {
        const std::array schema{mwtl::SettingDefinition{L"ProgressStep", mwtl::SettingType::dword},
                                mwtl::SettingDefinition{L"AppIdentity", mwtl::SettingType::string}};
        const auto result = settings_.Load(schema);
        Log(result ? L"Typed settings loaded." : L"No valid saved settings (status " +
            std::to_wstring(static_cast<int>(result.status)) + L").");
    }

    void ClearSettings() {
        const std::array<std::wstring_view, 2> names{L"ProgressStep", L"AppIdentity"};
        const auto result = settings_.RemoveOwned(names);
        Log(result ? L"Owned settings removed." : L"Settings cleanup status: " +
            std::to_wstring(static_cast<int>(result.status)));
    }

    void RunSelfTest() noexcept {
        int result = 0;
        try {
            const std::wstring isolated = L"Software\\mwtl\\Tests\\ShellDemo-" +
                                          std::to_wstring(::GetCurrentProcessId());
            auto spec = Association();
            spec.extension = L".isolated";
            spec.prog_id = L"mwtl.tests.shell-demo.document";
            auto registered = mwtl::RegisterFileAssociation(HKEY_CURRENT_USER, isolated, spec, false);
            if (!registered) result = 1;
            auto removed = mwtl::RemoveFileAssociation(HKEY_CURRENT_USER, isolated, spec, false);
            if (result == 0 && !removed) result = 2;
            ::RegDeleteTreeW(HKEY_CURRENT_USER, isolated.c_str());

            mwtl::TaskbarProgressModel model;
            if (result == 0 && (!model.SetValue(1, 4) || !model.IsValid())) result = 3;
            const std::array tasks{mwtl::JumpListTask{L"one", L"One", {}, {}, 0},
                                   mwtl::JumpListTask{L"two", L"Two", {}, {}, 0}};
            const std::array<std::wstring, 1> removed_ids{L"one"};
            const auto plan = mwtl::BuildJumpListPlan(tasks, removed_ids, 4);
            if (result == 0 && (!plan.valid || plan.tasks.size() != 1 ||
                                plan.tasks.front().id != L"two")) result = 4;
            const auto applied = taskbar_.Apply(model);
            if (result == 0 && applied.status != mwtl::ShellStatus::success &&
                applied.status != mwtl::ShellStatus::unavailable &&
                applied.status != mwtl::ShellStatus::com_failure) result = 5;
            const mwtl::HelpRequest safe{
                mwtl::HelpTargetKind::https_uri, {}, L"https://example.invalid/help", L"#taskbar"};
            if (result == 0 && !mwtl::ValidateHelpRequest(safe)) result = 6;
            const auto offline = mwtl::LaunchHelpWithBackend(GetHwnd(), safe,
                [](HWND, const mwtl::HelpRequest&) {
                    return mwtl::HelpResult{mwtl::HelpStatus::cancelled, ERROR_CANCELLED};
                });
            if (result == 0 && offline.status != mwtl::HelpStatus::cancelled) result = 7;
            taskbar_.Clear();
        } catch (...) { result = 8; }
        ::PostQuitMessage(result);
    }

    mwtl::VersionedSettingsStore settings_{HKEY_CURRENT_USER, kSettingsKey, 1};
    mwtl::TaskbarWindowIntegration taskbar_;
    mwtl::TaskbarProgressModel progress_model_;
    std::uint64_t progress_value_ = 0;
    bool overlay_visible_ = false;
    mwtl::Label title_, summary_, status_;
    mwtl::Button register_, remove_, jump_list_, remove_jump_list_, recent_, progress_, overlay_,
        save_, clear_settings_, help_;
    mwtl::TextBox log_;
};
}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
    g_self_test = std::wstring_view{::GetCommandLineW()}.find(L"--self-test") !=
                  std::wstring_view::npos;
    return mwtl::RunApplication<ShellIntegrationWindow>(
        instance, show_command, {}, {.com_apartment = mwtl::ComApartment::sta});
}
