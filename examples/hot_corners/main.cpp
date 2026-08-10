#include <mwfl/mwfl.h>

#include "hot_corner_model.h"

#include <shellapi.h>
#include <oleacc.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <stdexcept>
#include <string>
#include <vector>

using namespace mwfl;
using namespace std::chrono_literals;

namespace {

constexpr ControlId kEnabled{100};
constexpr ControlId kMonitor{101};
constexpr ControlId kFirstAction{110};
constexpr ControlId kDwell{120};
constexpr ControlId kTolerance{121};
constexpr ControlId kFullscreen{122};
constexpr ControlId kStatus{130};
constexpr TimerId kPoll{1};
constexpr TimerId kSelfTest{2};
constexpr UINT kToggleCommand = 200;
constexpr UINT kExitCommand = 201;
constexpr UINT kTrayMessage = WM_APP + 42;
constexpr GUID kTrayIdentity{0x6ca0ba3d,
                             0xb00e,
                             0x483f,
                             {0xa0, 0xe3, 0x95, 0xe2, 0xb1, 0xad, 0x5c, 0x8f}};
constexpr wchar_t kRegistryKey[] = L"Software\\mwfl\\Examples\\HotCorners";
constexpr std::array<std::uint32_t, 4> kDwellValues{200, 350, 500, 750};
constexpr std::array<LONG, 4> kToleranceValues{1, 2, 4, 8};

bool g_test_mode = false;
bool g_self_test = false;

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

const wchar_t* ActionName(hot_corners::Action action) noexcept {
    switch (action) {
    case hot_corners::Action::none: return L"Disabled";
    case hot_corners::Action::task_view: return L"Task View";
    case hot_corners::Action::notifications: return L"Notifications";
    case hot_corners::Action::start: return L"Start";
    case hot_corners::Action::show_desktop: return L"Show Desktop";
    }
    return L"Disabled";
}

void SendAction(hot_corners::Action action) noexcept {
    if (g_test_mode || action == hot_corners::Action::none) return;
    WORD key = 0;
    switch (action) {
    case hot_corners::Action::task_view: key = VK_TAB; break;
    case hot_corners::Action::notifications: key = 'N'; break;
    case hot_corners::Action::start: break;
    case hot_corners::Action::show_desktop: key = 'D'; break;
    case hot_corners::Action::none: return;
    }
    std::array<INPUT, 4> inputs{};
    UINT count = 0;
    inputs[count].type = INPUT_KEYBOARD;
    inputs[count++].ki.wVk = VK_LWIN;
    if (key != 0) {
        inputs[count].type = INPUT_KEYBOARD;
        inputs[count++].ki.wVk = key;
        inputs[count].type = INPUT_KEYBOARD;
        inputs[count].ki.wVk = key;
        inputs[count++].ki.dwFlags = KEYEVENTF_KEYUP;
    }
    inputs[count].type = INPUT_KEYBOARD;
    inputs[count].ki.wVk = VK_LWIN;
    inputs[count++].ki.dwFlags = KEYEVENTF_KEYUP;
    ::SendInput(count, inputs.data(), sizeof(INPUT));
}

BOOL CALLBACK CollectMonitor(HMONITOR monitor, HDC, LPRECT, LPARAM data) {
    auto& areas = *reinterpret_cast<std::vector<RECT>*>(data);
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    if (::GetMonitorInfoW(monitor, &info) != FALSE) areas.push_back(info.rcMonitor);
    return TRUE;
}

bool IsFullscreenBusy() noexcept {
    QUERY_USER_NOTIFICATION_STATE state = QUNS_NOT_PRESENT;
    if (FAILED(::SHQueryUserNotificationState(&state))) return false;
    return state == QUNS_RUNNING_D3D_FULL_SCREEN || state == QUNS_BUSY ||
           state == QUNS_PRESENTATION_MODE;
}

class HotCornersWindow final : public WindowBase {
public:
    void BuildUI() override {
        SetTitle(L"mwfl Hot Corners - x64 multi-monitor reference");
        BuildMenu();
        CreateControls();
        RefreshMonitors();
        LoadSettings();
        PopulateSettingsControls();
        ApplyFont(GetDpiContext().GetDpi());
        if (!AddTrayIcon()) throw std::runtime_error("tray icon creation failed");
        if (!poll_.Start(*this, kPoll, 30ms)) throw std::runtime_error("poll timer creation failed");
        if (g_self_test && ::SetTimer(GetHwnd(), kSelfTest.value, 100, nullptr) == 0)
            throw std::runtime_error("self-test timer creation failed");
        SavedWindowPlacement saved{};
        if (LoadWindowPlacementFromRegistry(HKEY_CURRENT_USER, kRegistryKey, L"MainWindow", saved))
            RestoreWindowPlacement(GetHwnd(), saved);
    }

    EventResult OnCommand(const CommandEvent& event) override {
        if (event.IsClicked(enabled_)) {
            settings_.enabled = enabled_.IsChecked();
            tracker_.Reset(); SaveSettings(); UpdateStatus();
            return EventResult::Handled();
        }
        if (event.IsClicked(fullscreen_)) {
            settings_.pause_for_fullscreen = fullscreen_.IsChecked();
            SaveSettings(); UpdateStatus();
            return EventResult::Handled();
        }
        if (event.Is(monitor_, CBN_SELCHANGE)) {
            StoreVisibleMonitor();
            shown_monitor_ = static_cast<std::size_t>((std::max)(0, monitor_.GetSelection()));
            LoadVisibleMonitor();
            return EventResult::Handled();
        }
        for (const auto& combo : actions_) {
            if (event.Is(combo, CBN_SELCHANGE)) {
                StoreVisibleMonitor(); SaveSettings(); return EventResult::Handled();
            }
        }
        if (event.Is(dwell_, CBN_SELCHANGE)) {
            settings_.dwell_ms = kDwellValues[static_cast<std::size_t>((std::max)(0, dwell_.GetSelection()))];
            tracker_.Reset(); SaveSettings(); UpdateStatus(); return EventResult::Handled();
        }
        if (event.Is(tolerance_, CBN_SELCHANGE)) {
            settings_.tolerance = kToleranceValues[static_cast<std::size_t>((std::max)(0, tolerance_.GetSelection()))];
            tracker_.Reset(); SaveSettings(); UpdateStatus(); return EventResult::Handled();
        }
        if (event.control == nullptr) return commands_.Dispatch(event);
        return EventResult::Propagate();
    }

    EventResult OnClose() override {
        StoreVisibleMonitor(); SaveSettings(); tray_.Remove();
        SavedWindowPlacement saved{};
        if (CaptureWindowPlacement(GetHwnd(), saved))
            SaveWindowPlacementToRegistry(HKEY_CURRENT_USER, kRegistryKey, L"MainWindow", saved);
        return EventResult::Propagate();
    }

    EventResult OnTimer(TimerId id) override {
        if (id == kSelfTest) {
            ::KillTimer(GetHwnd(), kSelfTest.value);
            try {
                RunSelfTest();
            } catch (...) {
                ::PostQuitMessage(self_test_step_);
            }
            return EventResult::Handled();
        }
        if (id != kPoll) return EventResult::Propagate();
        const bool fullscreen_paused = settings_.pause_for_fullscreen && IsFullscreenBusy();
        if (!settings_.enabled || manual_paused_ || fullscreen_paused) {
            tracker_.Reset();
            if (fullscreen_paused != last_fullscreen_paused_) { last_fullscreen_paused_ = fullscreen_paused; UpdateStatus(); }
            return EventResult::Handled();
        }
        if (last_fullscreen_paused_) { last_fullscreen_paused_ = false; UpdateStatus(); }
        POINT cursor{};
        if (::GetCursorPos(&cursor) == FALSE) return EventResult::Handled();
        const auto fired = tracker_.Update(
            hot_corners::Detect(cursor, monitors_, settings_.tolerance),
            ::GetTickCount64(), settings_.dwell_ms);
        if (fired) Activate(*fired);
        return EventResult::Handled();
    }

    EventResult OnMessage(const WindowMessage& event) override {
        if (event.id == WM_SETTINGCHANGE || event.id == WM_THEMECHANGED) {
            static_cast<void>(ApplyWindowAppearance(
                GetHwnd(), {ColorMode::system, Backdrop::mica}));
            ::RedrawWindow(GetHwnd(), nullptr, nullptr,
                           RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
        }
        if (event.id == WM_DISPLAYCHANGE || event.id == WM_SETTINGCHANGE) {
            StoreVisibleMonitor(); RefreshMonitors(); PopulateMonitorList(); LoadVisibleMonitor();
            return EventResult::Handled();
        }
        if (event.id == WM_THEMECHANGED) return EventResult::Handled();
        if (tray_.IsTaskbarCreated(event)) {
            static_cast<void>(tray_.Recreate());
            return EventResult::Handled();
        }
        const auto tray_event = tray_.Decode(event);
        if (!tray_event) return EventResult::Propagate();
        if (tray_event->kind == TrayIconEventKind::primary_activate) {
            TogglePause();
        } else if (tray_event->kind == TrayIconEventKind::context_menu) {
            ShowTrayMenu(tray_event->screen_position);
        } else if (tray_event->kind == TrayIconEventKind::balloon_clicked) {
            ::ShowWindow(GetHwnd(), SW_RESTORE);
            ::SetForegroundWindow(GetHwnd());
        }
        return EventResult::Handled();
    }

    EventResult OnDpiChanged(const DpiChangedEvent& event) override { ApplyFont(event.dpi_x); return EventResult::Propagate(); }

private:
    void BuildMenu() {
        commands_.Add(Command({static_cast<int>(kToggleCommand)},
            L"&Pause / Resume\tCtrl+E", [this] {
                TogglePause();
            }).SetShortcut({FVIRTKEY | FCONTROL, 'E'}));
        commands_.Add(Command({static_cast<int>(kExitCommand)},
            L"E&xit\tCtrl+Q", [this] { static_cast<void>(Close()); })
            .SetShortcut({FVIRTKEY | FCONTROL, 'Q'}));
        Menu bar; Menu app;
        if (!bar.Create() || !app.CreatePopup() ||
            !app.AppendCommand(*commands_.Find({static_cast<int>(kToggleCommand)})) ||
            !app.AppendSeparator() ||
            !app.AppendCommand(*commands_.Find({static_cast<int>(kExitCommand)})) ||
            !bar.AppendSubmenu(std::move(app), L"&Hot Corners") || !bar.AttachToWindow(GetHwnd()))
            throw std::runtime_error("menu creation failed");
        if (!accelerators_.Create(commands_))
            throw std::runtime_error("accelerator creation failed");
        SetAccelerators(accelerators_.GetHandle());
    }

    void CreateControls() {
        ControlHost ui{*this};
        ui.Add(enabled_, kEnabled, L"Enabled", {16_dip, 16_dip, 120_dip, 24_dip});
        ui.Add(fullscreen_, kFullscreen, L"Pause for fullscreen apps", {150_dip, 16_dip, 220_dip, 24_dip});
        ui.Add(monitor_, kMonitor, {16_dip, 56_dip, 220_dip, 180_dip});
        constexpr std::array<const wchar_t*, 4> corners{L"Top left", L"Top right", L"Bottom left", L"Bottom right"};
        for (std::size_t i = 0; i < 4; ++i) {
            ui.Add(corner_labels_[i], {static_cast<int>(140 + i)}, corners[i],
                {260_dip, Dip{56.0f + static_cast<float>(i) * 42.0f}, 110_dip, 24_dip});
            ui.Add(actions_[i], {static_cast<int>(kFirstAction.value + i)},
                {380_dip, Dip{52.0f + static_cast<float>(i) * 42.0f}, 190_dip, 180_dip});
            const auto populated = AddItems(actions_[i], {
                ActionName(hot_corners::Action::none),
                ActionName(hot_corners::Action::task_view),
                ActionName(hot_corners::Action::notifications),
                ActionName(hot_corners::Action::start),
                ActionName(hot_corners::Action::show_desktop)});
            if (!populated) throw std::runtime_error("action population failed");
        }
        ui.Add(dwell_label_, {150}, L"Dwell", {16_dip, 246_dip, 80_dip, 24_dip});
        ui.Add(dwell_, kDwell, {96_dip, 242_dip, 140_dip, 150_dip});
        for (auto value : kDwellValues) dwell_.AddItem(std::to_wstring(value) + L" ms");
        ui.Add(tolerance_label_, {151}, L"Tolerance", {260_dip, 246_dip, 100_dip, 24_dip});
        ui.Add(tolerance_, kTolerance, {360_dip, 242_dip, 140_dip, 150_dip});
        for (auto value : kToleranceValues) tolerance_.AddItem(std::to_wstring(value) + L" px");
        ui.Add(status_, kStatus, L"Starting...", {16_dip, 302_dip, 650_dip, 48_dip});
        Must(SetAccessibleName(monitor_.GetHwnd(), L"Display monitor"), "name monitor selector");
        Must(SetAccessibleName(dwell_.GetHwnd(), L"Activation dwell time"), "name dwell selector");
        Must(SetAccessibleName(tolerance_.GetHwnd(), L"Corner tolerance"),
             "name tolerance selector");
        Must(SetAccessibleName(status_.GetHwnd(), L"Hot Corners status"), "name status");
        for (std::size_t i = 0; i < actions_.size(); ++i) {
            const std::wstring accessible_name = std::wstring(corners[i]) + L" action";
            Must(SetAccessibleName(actions_[i].GetHwnd(), accessible_name.c_str()),
                 "name corner action selector");
        }
    }

    void RunSelfTest() {
        self_test_step_ = 2;
        if (!HasAccessibleName(monitor_.GetHwnd(), L"Display monitor") ||
            !HasAccessibleName(dwell_.GetHwnd(), L"Activation dwell time") ||
            !HasAccessibleName(actions_[0].GetHwnd(), L"Top left action") ||
            !HasAccessibleName(status_.GetHwnd(), L"Hot Corners status"))
            throw std::runtime_error("Hot Corners accessible names mismatch");

        self_test_step_ = 3;
        std::array<ACCEL, 2> entries{};
        const int accelerator_count =
            ::CopyAcceleratorTableW(accelerators_.GetHandle(), entries.data(),
                                    static_cast<int>(entries.size()));
        const bool has_toggle = std::ranges::any_of(entries.begin(),
                                                    entries.begin() + accelerator_count,
                                                    [](const ACCEL& entry) {
                                                        return entry.cmd == kToggleCommand &&
                                                               entry.key == 'E' &&
                                                               (entry.fVirt & FCONTROL) != 0;
                                                    });
        if (!has_toggle ||
            ::SendMessageW(GetHwnd(), WM_COMMAND, kToggleCommand, 0) != 0 ||
            !manual_paused_ || status_.GetText().find(L"Paused") == std::wstring::npos)
            throw std::runtime_error("Hot Corners keyboard command failed");
        if (::SendMessageW(GetHwnd(), WM_COMMAND, kToggleCommand, 0) != 0 ||
            manual_paused_)
            throw std::runtime_error("Hot Corners keyboard command did not restore state");

        self_test_step_ = 4;
        ApplyFont(GetDpiContext().GetDpi());
        if (::SendMessageW(enabled_.GetHwnd(), WM_GETFONT, 0, 0) == 0 ||
            ::SendMessageW(actions_[0].GetHwnd(), WM_GETFONT, 0, 0) == 0)
            throw std::runtime_error("Hot Corners DPI font projection failed");
        ::SendMessageW(GetHwnd(), WM_THEMECHANGED, 0, 0);
        ::SendMessageW(GetHwnd(), WM_SETTINGCHANGE, 0, 0);
        if (!enabled_.IsWindow() || !monitor_.IsWindow() || !status_.IsWindow())
            throw std::runtime_error("Hot Corners appearance refresh destroyed controls");

        self_test_step_ = 5;
        RECT status_bounds{};
        if (::GetWindowRect(status_.GetHwnd(), &status_bounds) == FALSE ||
            status_bounds.right <= status_bounds.left || status_bounds.bottom <= status_bounds.top)
            throw std::runtime_error("Hot Corners retained layout is invalid");
        Activate({0, hot_corners::Corner::top_left});
        Close();
    }

    void RefreshMonitors() {
        monitors_.clear();
        ::EnumDisplayMonitors(nullptr, nullptr, CollectMonitor, reinterpret_cast<LPARAM>(&monitors_));
        settings_.monitors.resize(monitors_.size());
        shown_monitor_ = (std::min)(shown_monitor_, monitors_.empty() ? std::size_t{0} : monitors_.size() - 1);
        tracker_.Reset();
    }

    void PopulateSettingsControls() {
        ::SendMessageW(enabled_.GetHwnd(), BM_SETCHECK, settings_.enabled ? BST_CHECKED : BST_UNCHECKED, 0);
        ::SendMessageW(fullscreen_.GetHwnd(), BM_SETCHECK, settings_.pause_for_fullscreen ? BST_CHECKED : BST_UNCHECKED, 0);
        PopulateMonitorList();
        const auto dwell = std::find(kDwellValues.begin(), kDwellValues.end(), settings_.dwell_ms);
        dwell_.SetSelection(dwell == kDwellValues.end() ? 1 : static_cast<int>(dwell - kDwellValues.begin()));
        const auto tolerance = std::find(kToleranceValues.begin(), kToleranceValues.end(), settings_.tolerance);
        tolerance_.SetSelection(tolerance == kToleranceValues.end() ? 1 : static_cast<int>(tolerance - kToleranceValues.begin()));
        LoadVisibleMonitor(); UpdateStatus();
    }

    void PopulateMonitorList() {
        ::SendMessageW(monitor_.GetHwnd(), CB_RESETCONTENT, 0, 0);
        for (std::size_t i = 0; i < monitors_.size(); ++i)
            monitor_.AddItem(L"Monitor " + std::to_wstring(i + 1));
        if (!monitors_.empty()) monitor_.SetSelection(static_cast<int>(shown_monitor_));
    }

    void LoadVisibleMonitor() {
        if (shown_monitor_ >= settings_.monitors.size()) return;
        for (std::size_t i = 0; i < 4; ++i)
            actions_[i].SetSelection(static_cast<int>(settings_.monitors[shown_monitor_].corners[i]));
    }

    void StoreVisibleMonitor() {
        if (shown_monitor_ >= settings_.monitors.size()) return;
        for (std::size_t i = 0; i < 4; ++i) {
            const int selected = actions_[i].GetSelection();
            if (selected >= 0 && selected <= 4)
                settings_.monitors[shown_monitor_].corners[i] = static_cast<hot_corners::Action>(selected);
        }
    }

    void LoadSettings() {
        HKEY key = nullptr;
        if (::RegOpenKeyExW(HKEY_CURRENT_USER, kRegistryKey, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) return;
        auto close = wil::scope_exit([&] { ::RegCloseKey(key); });
        auto read = [&](const std::wstring& name, DWORD fallback) {
            DWORD value = fallback, bytes = sizeof(value), type = 0;
            return ::RegQueryValueExW(key, name.c_str(), nullptr, &type, reinterpret_cast<BYTE*>(&value), &bytes) == ERROR_SUCCESS && type == REG_DWORD ? value : fallback;
        };
        settings_.dwell_ms = read(L"DwellMs", settings_.dwell_ms);
        settings_.tolerance = static_cast<LONG>(read(L"Tolerance", settings_.tolerance));
        settings_.enabled = read(L"Enabled", 1) != 0;
        settings_.pause_for_fullscreen = read(L"PauseFullscreen", 1) != 0;
        for (std::size_t m = 0; m < settings_.monitors.size(); ++m)
            for (std::size_t c = 0; c < 4; ++c)
                settings_.monitors[m].corners[c] = static_cast<hot_corners::Action>(
                    (std::min<DWORD>)(4, read(L"Monitor" + std::to_wstring(m) + L"Corner" + std::to_wstring(c), static_cast<DWORD>(settings_.monitors[m].corners[c]))));
    }

    void SaveSettings() const {
        HKEY key = nullptr; DWORD disposition = 0;
        if (::RegCreateKeyExW(HKEY_CURRENT_USER, kRegistryKey, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, &disposition) != ERROR_SUCCESS) return;
        auto close = wil::scope_exit([&] { ::RegCloseKey(key); });
        auto write = [&](const std::wstring& name, DWORD value) {
            ::RegSetValueExW(key, name.c_str(), 0, REG_DWORD, reinterpret_cast<const BYTE*>(&value), sizeof(value));
        };
        write(L"DwellMs", settings_.dwell_ms); write(L"Tolerance", settings_.tolerance);
        write(L"Enabled", settings_.enabled); write(L"PauseFullscreen", settings_.pause_for_fullscreen);
        for (std::size_t m = 0; m < settings_.monitors.size(); ++m)
            for (std::size_t c = 0; c < 4; ++c)
                write(L"Monitor" + std::to_wstring(m) + L"Corner" + std::to_wstring(c), static_cast<DWORD>(settings_.monitors[m].corners[c]));
    }

    void Activate(const hot_corners::Hit& hit) {
        const auto action = hot_corners::ResolveAction(settings_, hit);
        status_.SetText((g_test_mode ? L"TEST (input suppressed): " : L"") +
            std::wstring(L"Monitor ") + std::to_wstring(hit.monitor + 1) + L" - " + ActionName(action));
        SendAction(action);
    }

    void UpdateStatus() {
        std::wstring state = !settings_.enabled ? L"Disabled" : manual_paused_ ? L"Paused from tray/menu" :
            last_fullscreen_paused_ ? L"Paused for fullscreen app" : L"Watching";
        if (g_test_mode) state += L" (test mode: input suppressed)";
        status_.SetText(state + L"; " + std::to_wstring(monitors_.size()) + L" monitor(s), " +
            std::to_wstring(settings_.dwell_ms) + L" ms / " + std::to_wstring(settings_.tolerance) + L" px");
    }

    bool AddTrayIcon() {
        return tray_.Add({.owner = GetHwnd(),
                          .id = 1,
                          .callback_message = kTrayMessage,
                          .identity = kTrayIdentity,
                          .icon = ::LoadIconW(nullptr, IDI_APPLICATION),
                          .tooltip = L"mwfl Hot Corners - Watching"});
    }

    void TogglePause() {
        manual_paused_ = !manual_paused_;
        tracker_.Reset();
        UpdateStatus();
        const std::wstring state = manual_paused_ ? L"Paused" : L"Watching";
        static_cast<void>(tray_.UpdateTooltip(L"mwfl Hot Corners - " + state));
        static_cast<void>(tray_.ShowNotification(
            {.title = L"mwfl Hot Corners", .text = state, .respect_quiet_time = true}));
    }

    void ShowTrayMenu(POINT point) {
        Command* toggle = commands_.Find({static_cast<int>(kToggleCommand)});
        Command* exit = commands_.Find({static_cast<int>(kExitCommand)});
        if (toggle == nullptr || exit == nullptr) return;
        toggle->SetText(manual_paused_ ? L"Resume" : L"Pause");
        Menu menu; if (!menu.CreatePopup() || !menu.AppendCommand(*toggle) ||
            !menu.AppendSeparator() || !menu.AppendCommand(*exit)) return;
        if (point.x == -1 && point.y == -1) ::GetCursorPos(&point);
        ::SetForegroundWindow(GetHwnd());
        const PopupMenuResult selected = menu.TrackResult(GetHwnd(), point);
        if (selected) ::PostMessageW(GetHwnd(), WM_COMMAND, selected.command, 0);
    }

    void ApplyFont(UINT dpi) {
        if (!font_.CreateMessageFont(dpi)) return;
        for (HWND control : {enabled_.GetHwnd(), fullscreen_.GetHwnd(), monitor_.GetHwnd(), dwell_.GetHwnd(), tolerance_.GetHwnd(), status_.GetHwnd()})
            SetControlFont(control, font_.GetHandle());
        for (std::size_t i = 0; i < 4; ++i) { SetControlFont(corner_labels_[i].GetHwnd(), font_.GetHandle()); SetControlFont(actions_[i].GetHwnd(), font_.GetHandle()); }
    }

    CheckBox enabled_, fullscreen_;
    ComboBox monitor_, dwell_, tolerance_;
    std::array<Label, 4> corner_labels_;
    std::array<ComboBox, 4> actions_;
    Label dwell_label_, tolerance_label_, status_;
    UiTimer poll_;
    UiFont font_;
    AcceleratorTable accelerators_;
    TrayIcon tray_;
    CommandSet commands_;
    std::vector<RECT> monitors_;
    hot_corners::Settings settings_;
    hot_corners::DwellTracker tracker_;
    std::size_t shown_monitor_ = 0;
    bool manual_paused_ = false, last_fullscreen_paused_ = false;
    int self_test_step_ = 1;
};

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    g_self_test = wcsstr(::GetCommandLineW(), L"--self-test") != nullptr;
    g_test_mode = g_self_test || wcsstr(::GetCommandLineW(), L"--test-mode") != nullptr;
    WindowOptions options{};
    options.title = L"mwfl Hot Corners";
    options.initial_bounds = {{0_dip, 0_dip}, {700_dip, 410_dip}};
    options.use_default_bounds = false;
    options.appearance.color_mode = ColorMode::system;
    options.appearance.backdrop = Backdrop::mica;
    return RunApplication<HotCornersWindow>(instance, show, options);
}
