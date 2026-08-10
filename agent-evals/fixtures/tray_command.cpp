#include <mwtl/mwtl.h>

#include <stdexcept>

namespace {

constexpr UINT kTrayMessage = WM_APP + 52;
constexpr mwtl::ControlId kPause{620};
constexpr GUID kTrayGuid{
    0x6d2a6c84, 0x67b5, 0x4eae, {0x82, 0x88, 0x7d, 0x04, 0x6a, 0x42, 0xd4, 0x7f}};

class TrayCommandWindow final : public mwtl::WindowBase {
   public:
    void BuildUI() override {
        commands_.Add(mwtl::Command(kPause, L"Pause", [this] { TogglePaused(); }));
        if (!tray_.Add({.owner = GetHwnd(),
                        .id = 1,
                        .callback_message = kTrayMessage,
                        .identity = kTrayGuid,
                        .icon = ::LoadIconW(nullptr, IDI_APPLICATION),
                        .tooltip = L"Agent eval - Running"})) {
            throw std::runtime_error("tray icon creation failed");
        }
    }

    mwtl::EventResult OnCommand(const mwtl::CommandEvent& event) override {
        return commands_.Dispatch(event);
    }

    mwtl::EventResult OnMessage(const mwtl::WindowMessage& message) override {
        if (tray_.IsTaskbarCreated(message)) {
            static_cast<void>(tray_.Recreate());
            return mwtl::EventResult::Handled();
        }
        const auto event = tray_.Decode(message);
        if (!event) return mwtl::EventResult::Propagate();
        if (event->kind == mwtl::TrayIconEventKind::primary_activate) {
            TogglePaused();
        } else if (event->kind == mwtl::TrayIconEventKind::context_menu) {
            ShowTrayMenu(event->screen_position);
        }
        return mwtl::EventResult::Handled();
    }

    mwtl::EventResult OnClose() override {
        tray_.Remove();
        return mwtl::EventResult::Propagate();
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
        mwtl::Menu menu;
        if (pause == nullptr || !menu.CreatePopup() || !menu.AppendCommand(*pause)) return;
        if (position.x == -1 && position.y == -1) ::GetCursorPos(&position);
        ::SetForegroundWindow(GetHwnd());
        const UINT selected = menu.Track(GetHwnd(), position);
        if (selected != 0) ::PostMessageW(GetHwnd(), WM_COMMAND, selected, 0);
    }

    bool paused_ = false;
    mwtl::CommandSet commands_;
    mwtl::TrayIcon tray_;
};

}  // namespace
