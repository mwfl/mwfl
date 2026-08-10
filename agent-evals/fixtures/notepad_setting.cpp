#include <mwfl/mwfl.h>

#include <string>
#include <string_view>

namespace {
constexpr mwfl::ControlId kAlwaysOnTop{718};
constexpr std::wstring_view kSettingsKey = L"Software\\Example\\Notepad\\1";

bool LoadBool(HKEY root, std::wstring_view subkey,
              std::wstring_view name, bool fallback) noexcept {
    DWORD value{};
    DWORD bytes = sizeof(value);
    const std::wstring key{subkey};
    const std::wstring value_name{name};
    return ::RegGetValueW(root, key.c_str(), value_name.c_str(),
                          RRF_RT_REG_DWORD, nullptr, &value, &bytes) == ERROR_SUCCESS &&
            bytes == sizeof(value)
        ? value != 0
        : fallback;
}

bool SaveBool(HKEY root, std::wstring_view subkey,
              std::wstring_view name, bool value) noexcept {
    const std::wstring key{subkey};
    HKEY opened{};
    if (::RegCreateKeyExW(root, key.c_str(), 0, nullptr, 0, KEY_SET_VALUE,
                          nullptr, &opened, nullptr) != ERROR_SUCCESS) return false;
    const DWORD stored = value ? 1u : 0u;
    const std::wstring value_name{name};
    const LSTATUS status = ::RegSetValueExW(
        opened, value_name.c_str(), 0, REG_DWORD,
        reinterpret_cast<const BYTE*>(&stored), sizeof(stored));
    ::RegCloseKey(opened);
    return status == ERROR_SUCCESS;
}
}  // namespace

class EvalNotepadSetting final : public mwfl::WindowBase {
public:
    void BuildUI() override {
        topmost_ = LoadBool(
            HKEY_CURRENT_USER, kSettingsKey, L"AlwaysOnTop", false);
        commands_.Add(mwfl::Command(kAlwaysOnTop, L"Always on &Top", [this] {
            topmost_ = !topmost_;
            ApplyTopmost();
            static_cast<void>(SaveBool(
                HKEY_CURRENT_USER, kSettingsKey, L"AlwaysOnTop", topmost_));
        }).SetChecked(topmost_));

        mwfl::Menu view;
        mwfl::Must(menu_.Create(), "create menu bar");
        mwfl::Must(view.CreatePopup(), "create View menu");
        mwfl::Must(view.AppendCommand(*commands_.Find(kAlwaysOnTop)),
                   "append topmost command");
        mwfl::Must(menu_.AppendSubmenu(std::move(view), L"&View"),
                   "append View menu");
        mwfl::Must(menu_.AttachToWindow(GetHwnd()), "attach menu bar");
        ApplyTopmost();
    }

    mwfl::EventResult OnCommand(const mwfl::CommandEvent& event) override {
        return commands_.Dispatch(event);
    }

private:
    void ApplyTopmost() {
        mwfl::Must(::SetWindowPos(
            GetHwnd(), topmost_ ? HWND_TOPMOST : HWND_NOTOPMOST,
            0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE) != FALSE,
            "apply topmost state");
        auto* command = commands_.Find(kAlwaysOnTop);
        command->SetChecked(topmost_);
        mwfl::Must(menu_.UpdateCommand(*command), "update menu command");
    }

    bool topmost_{};
    mwfl::CommandSet commands_;
    mwfl::Menu menu_;
};
