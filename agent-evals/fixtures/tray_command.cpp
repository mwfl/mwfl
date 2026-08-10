#include <mwfl/mwfl.h>

#include <stdexcept>

namespace {

constexpr UINT kTrayMessage = WM_APP + 52;
constexpr mwfl::ControlId kPause{620};
constexpr GUID kTrayGuid{
    0x6d2a6c84, 0x67b5, 0x4eae, {0x82, 0x88, 0x7d, 0x04, 0x6a, 0x42, 0xd4, 0x7f}};

class TrayCommandWindow final : public mwfl::WindowBase {
   public:
    void BuildUI() override {
        commands_.Add(mwfl::Command(kPause, L"Pause", [this] { TogglePaused(); }));
        if (!tray_.Add({.owner = GetHwnd(),
                        .id = 1,
                        .callback_message = kTrayMessage,
                        .identity = kTrayGuid,
                        .icon = ::LoadIconW(nullptr, IDI_APPLICATION),
                        .tooltip = L"Agent eval - Running"})) {
            throw std::runtime_error("tray icon creation failed");
        }
    }

    mwfl::EventResult OnCommand(const mwfl::CommandEvent& event) override {
        return commands_.Dispatch(event);
    }

    mwfl::EventResult OnMessage(const mwfl::WindowMessage& message) override {
        if (tray_.IsTaskbarCreated(message)) {
            static_cast<void>(tray_.Recreate());
            return mwfl::EventResult::Handled();
        }
        const auto event = tray_.Decode(message);
        if (!event) return mwfl::EventResult::Propagate();
        if (event->kind == mwfl::TrayIconEventKind::primary_activate) {
            TogglePaused();
        } else if (event->kind == mwfl::TrayIconEventKind::context_menu) {
            ShowTrayMenu(event->screen_position);
        }
        return mwfl::EventResult::Handled();
    }

    mwfl::EventResult OnClose() override {
        tray_.Remove();
        return mwfl::EventResult::Propagate();
    }

   private:
    void TogglePaused() {
        paused_ = !paused_;
        if (auto* command = commands_.Find(kPause)) {
            command->SetChecked(paused_).SetText(paused_ ? L"Resume" : L"Pause");
        }
        static_cast<void>(
            tray_.UpdateTooltip(paused_ ? L"Agent eval - Paused" : L"Agent eval - Running"));
    }

    void ShowTrayMenu(POINT position) {
        const auto* pause = commands_.Find(kPause);
        mwfl::Menu menu;
        if (pause == nullptr || !menu.CreatePopup() || !menu.AppendCommand(*pause)) return;
        if (position.x == -1 && position.y == -1) ::GetCursorPos(&position);
        ::SetForegroundWindow(GetHwnd());
        const UINT selected = menu.Track(GetHwnd(), position);
        if (selected != 0) ::PostMessageW(GetHwnd(), WM_COMMAND, selected, 0);
    }

    bool paused_ = false;
    mwfl::CommandSet commands_;
    mwfl::TrayIcon tray_;
};

}  // namespace
