#include <mwfl/mwfl.h>

#include <string>

using mwfl::operator""_dip;

namespace {

class CommandsWindow final : public mwfl::WindowBase {
public:
    void BuildUI() override {
        SetTitle(L"Command studio - one action model, three surfaces");
        BuildCommands();

        mwfl::ControlHost ui{*this};
        ui.Add(title_, L"Command studio");
        ui.Add(subtitle_, L"Menu, toolbar, and keyboard shortcuts share the same command state.");
        ui.Add(toolbar_);
        mwfl::TextBoxOptions editor_options;
        editor_options.style |= ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL;
        ui.Add(editor_, L"Write something here, then save it.\r\n\r\nCtrl+N creates a fresh note. Ctrl+S saves. F11 toggles focus mode.", editor_options);
        ui.Add(inspector_, L"Command state");
        ui.Add(save_state_, L"Save is disabled until the document changes.");
        ui.Add(focus_state_, L"Focus mode: off");
        ui.Add(hint_, L"Try the toolbar, the Command menu, or the shortcuts - every route dispatches through CommandSet.");
        ui.Add(status_, L"Ready");

        for (const auto& command : commands_.GetCommands()) {
            if (command.GetId() != kExit) mwfl::Must(toolbar_.AddCommand(command), "add toolbar command");
        }
        toolbar_.AutoSize();
        BuildMenu();
        mwfl::Must(accelerators_.Create(commands_), "create accelerator table");
        SetAccelerators(accelerators_.GetHandle());

        ApplyFont(GetDpiContext().GetDpi());
        SetAppearance({mwfl::ColorMode::system, mwfl::Backdrop::mica});
        SetLayout(mwfl::Column().Margin(24.0_dip).Gap(10.0_dip)
            .Add(title_, mwfl::Fixed(34.0_dip))
            .Add(subtitle_, mwfl::Fixed(24.0_dip))
            .Add(toolbar_, mwfl::Fixed(38.0_dip))
            .Add(mwfl::Row().Gap(14.0_dip)
                .Add(editor_, mwfl::Stretch())
                .Add(mwfl::Overlay()
                    .Add(inspector_)
                    .Add(mwfl::Column().Margin({20.0_dip, 34.0_dip, 20.0_dip, 16.0_dip}).Gap(10.0_dip)
                        .Add(save_state_, mwfl::Auto())
                        .Add(focus_state_, mwfl::Auto())
                        .Add(hint_, mwfl::Stretch())), mwfl::Fixed(310.0_dip)),
                mwfl::Stretch())
            .Add(status_, mwfl::Fixed(28.0_dip)));
    }

    mwfl::EventResult OnCommand(const mwfl::CommandEvent& event) override {
        if (event.Is(editor_, EN_CHANGE) && !updating_) {
            dirty_ = true;
            SyncState(L"Document changed");
            return mwfl::EventResult::Handled();
        }
        return commands_.Dispatch(event);
    }

    mwfl::EventResult OnDpiChanged(const mwfl::DpiChangedEvent& event) override { ApplyFont(event.dpi_x); return mwfl::EventResult::Propagate(); }

private:
    static constexpr mwfl::ControlId kNew{600};
    static constexpr mwfl::ControlId kSave{601};
    static constexpr mwfl::ControlId kFocus{602};
    static constexpr mwfl::ControlId kExit{603};

    void BuildCommands() {
        commands_
            .Add(mwfl::Command(kNew, L"New note", [this] {
                updating_ = true;
                editor_.SetText(L"Untitled note\r\n\r\n");
                updating_ = false;
                dirty_ = false;
                editor_.Focus();
                SyncState(L"Created a fresh note");
            }).SetShortcut({FVIRTKEY | FCONTROL, 'N'}))
            .Add(mwfl::Command(kSave, L"Save", [this] {
                dirty_ = false;
                SyncState(L"Saved locally (demo)");
            }).SetEnabled(false).SetShortcut({FVIRTKEY | FCONTROL, 'S'}))
            .Add(mwfl::Command(kFocus, L"Focus mode", [this] {
                focus_mode_ = !focus_mode_;
                SyncState(focus_mode_ ? L"Focus mode enabled" : L"Focus mode disabled");
                editor_.Focus();
            }).SetShortcut({FVIRTKEY, VK_F11}))
            .Add(mwfl::Command(kExit, L"Exit", [this] { static_cast<void>(Close()); }));
    }

    void BuildMenu() {
        mwfl::Menu bar;
        mwfl::Menu popup;
        mwfl::Must(bar.Create(), "create menu bar");
        mwfl::Must(popup.CreatePopup(), "create command menu");
        menu_handle_ = popup.GetHandle();
        mwfl::Must(popup.AppendCommand(*commands_.Find(kNew)), "append new command");
        mwfl::Must(popup.AppendCommand(*commands_.Find(kSave)), "append save command");
        mwfl::Must(popup.AppendSeparator(), "append separator");
        mwfl::Must(popup.AppendCommand(*commands_.Find(kFocus)), "append focus command");
        mwfl::Must(popup.AppendSeparator(), "append separator");
        mwfl::Must(popup.AppendCommand(*commands_.Find(kExit)), "append exit command");
        mwfl::Must(bar.AppendSubmenu(std::move(popup), L"Command"), "append submenu");
        mwfl::Must(bar.AttachToWindow(GetHwnd()), "attach menu");
    }

    void SyncState(std::wstring_view message) {
        auto* save = commands_.Find(kSave);
        auto* focus = commands_.Find(kFocus);
        save->SetEnabled(dirty_);
        focus->SetChecked(focus_mode_);
        toolbar_.UpdateCommand(*save);
        toolbar_.UpdateCommand(*focus);
        if (menu_handle_) {
            ::EnableMenuItem(menu_handle_, kSave.value, MF_BYCOMMAND | (dirty_ ? MF_ENABLED : MF_GRAYED));
            ::CheckMenuItem(menu_handle_, kFocus.value, MF_BYCOMMAND | (focus_mode_ ? MF_CHECKED : MF_UNCHECKED));
            ::DrawMenuBar(GetHwnd());
        }
        save_state_.SetText(dirty_ ? L"Save is enabled: the document has changes." : L"Save is disabled: everything is up to date.");
        focus_state_.SetText(focus_mode_ ? L"Focus mode: on (checked everywhere)" : L"Focus mode: off");
        status_.SetText(message);
    }

    void ApplyFont(UINT dpi) {
        if (!font_.CreateMessageFont(dpi)) return;
        for (HWND child = ::GetWindow(GetHwnd(), GW_CHILD); child; child = ::GetWindow(child, GW_HWNDNEXT))
            ::SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(font_.GetHandle()), TRUE);
    }

    mwfl::CommandSet commands_;
    mwfl::AcceleratorTable accelerators_;
    HMENU menu_handle_{};
    bool dirty_{};
    bool focus_mode_{};
    bool updating_{};
    mwfl::Label title_, subtitle_, save_state_, focus_state_, hint_, status_;
    mwfl::Toolbar toolbar_;
    mwfl::TextBox editor_;
    mwfl::GroupBox inspector_;
    mwfl::UiFont font_;
};

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    return mwfl::RunApplication<CommandsWindow>(instance, show, {.title = L"Command studio", .initial_bounds = {{}, {1080.0_dip, 680.0_dip}}, .use_default_bounds = false});
}
