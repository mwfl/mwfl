#include <mwfl/mwfl.h>

#include <filesystem>
#include <string>

using mwfl::operator""_dip;

namespace {

class DesktopIntegrationWindow final : public mwfl::WindowBase {
   public:
    void BuildUI() override {
        SetTitle(L"Desktop integration toolbox");
        mwfl::ControlHost ui{*this};
        ui.Add(title_, L"Desktop integration");
        ui.Add(subtitle_,
               L"Modern dialogs, clipboard, drag and drop, and persistent placement in one native "
               L"window.");
        ui.Add(dialogs_, L"Files and folders");
        ui.Add(open_, L"Open file...");
        ui.Add(save_, L"Save as...");
        ui.Add(folder_, L"Choose folder...");
        ui.Add(clipboard_, L"Clipboard");
        ui.Add(copy_, L"Copy summary");
        ui.Add(paste_, L"Paste text");
        ui.Add(task_, L"Show task dialog");
        ui.Add(custom_, L"Show custom dialog");
        ui.Add(drop_zone_, L"Drop files anywhere on this window");
        ui.Add(path_label_, L"Latest desktop result");
        ui.Add(path_, L"Nothing selected yet");
        path_.SetReadOnly(true);
        ui.Add(activity_label_, L"Activity");
        mwfl::TextBoxOptions log_options;
        log_options.style |= ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL;
        ui.Add(activity_, IsShowcase()
                              ? L"Ready for the public-preview tour.\r\n"
                                L"Received 3 dropped file(s).\r\n"
                                L"Release assets are available to native file workflows."
                              : L"Ready. Your window position will be restored next time.",
               log_options);
        activity_.SetReadOnly(true);
        if (IsShowcase())
            path_.SetText(L"C:\\MWFL Showcase\\mwfl-0.1.2-windows-x64.zip");

        mwfl::EnableFileDrop(GetHwnd());
        SetAppearance({mwfl::ColorMode::light, mwfl::Backdrop::mica});
        mwfl::SetDialogDefaultButton(GetHwnd(), static_cast<UINT>(open_.GetId().value));
        ApplyFont(GetDpiContext().GetDpi());
        SetLayout(mwfl::Column()
                      .Margin(18.0_dip)
                      .Gap(8.0_dip)
                      .Add(title_, mwfl::Fixed(28.0_dip))
                      .Add(subtitle_, mwfl::Fixed(22.0_dip))
                      .Add(dialogs_, mwfl::Fixed(24.0_dip))
                      .Add(mwfl::Row()
                               .Gap(6.0_dip)
                               .Add(open_, mwfl::Fixed(96.0_dip))
                               .Add(save_, mwfl::Fixed(96.0_dip))
                               .Add(folder_, mwfl::Fixed(108.0_dip))
                               .Add(mwfl::Column(), mwfl::Stretch()),
                           mwfl::Fixed(30.0_dip))
                      .Add(clipboard_, mwfl::Fixed(24.0_dip))
                      .Add(mwfl::Row()
                               .Gap(6.0_dip)
                               .Add(copy_, mwfl::Fixed(108.0_dip))
                               .Add(paste_, mwfl::Fixed(96.0_dip))
                               .Add(task_, mwfl::Fixed(118.0_dip))
                               .Add(custom_, mwfl::Fixed(126.0_dip))
                               .Add(mwfl::Column(), mwfl::Stretch()),
                           mwfl::Fixed(30.0_dip))
                      .Add(drop_zone_, mwfl::Fixed(52.0_dip))
                      .Add(path_label_, mwfl::Auto())
                      .Add(path_, mwfl::Fixed(34.0_dip))
                      .Add(activity_label_, mwfl::Auto())
                      .Add(activity_, mwfl::Stretch()));

        mwfl::SavedWindowPlacement saved;
        if (!IsShowcase() &&
            mwfl::LoadWindowPlacementFromRegistry(HKEY_CURRENT_USER, kRegistryKey, L"Window",
                                                  saved))
            mwfl::RestoreWindowPlacement(GetHwnd(), saved);
    }

    mwfl::EventResult OnCommand(const mwfl::CommandEvent& event) override {
        if (event.IsClicked(open_)) {
            mwfl::FileDialogOptions options{GetHwnd(), L"Open a file with mwfl"};
            options.filters = {{L"Text files", L"*.txt;*.md"}, {L"All files", L"*.*"}};
            ShowResult(L"Open", mwfl::ShowOpenFileDialog(options));
            return mwfl::EventResult::Handled();
        }
        if (event.IsClicked(save_)) {
            mwfl::FileDialogOptions options{GetHwnd(), L"Choose a destination"};
            options.filters = {{L"Text document", L"*.txt"}, {L"All files", L"*.*"}};
            options.default_extension = L"txt";
            ShowResult(L"Save", mwfl::ShowSaveFileDialog(options));
            return mwfl::EventResult::Handled();
        }
        if (event.IsClicked(folder_)) {
            ShowResult(L"Folder",
                       mwfl::ShowFolderDialog({GetHwnd(), L"Choose a workspace folder"}));
            return mwfl::EventResult::Handled();
        }
        if (event.IsClicked(copy_)) {
            const auto value = path_.GetText();
            AppendActivity(mwfl::SetClipboardText(GetHwnd(), value) ? L"Copied the latest result."
                                                                    : L"Clipboard copy failed.");
            return mwfl::EventResult::Handled();
        }
        if (event.IsClicked(paste_)) {
            const auto text = mwfl::GetClipboardText(GetHwnd());
            path_.SetText(text.empty() ? L"Clipboard contains no Unicode text" : text);
            AppendActivity(L"Read Unicode text from the clipboard.");
            return mwfl::EventResult::Handled();
        }
        if (event.IsClicked(task_)) {
            mwfl::ShowTaskDialog(GetHwnd(), L"mwfl desktop integration",
                                 L"Native Windows shell features",
                                 L"These helpers preserve explicit ownership and report "
                                 L"cancellation separately from failure.");
            AppendActivity(L"Displayed a native task dialog.");
            return mwfl::EventResult::Handled();
        }
        if (event.IsClicked(custom_)) {
            ShowCustomDialog();
            return mwfl::EventResult::Handled();
        }
        return mwfl::EventResult::Propagate();
    }

    mwfl::EventResult OnMessage(const mwfl::WindowMessage& event) override {
        if (event.id != WM_DROPFILES) return mwfl::EventResult::Propagate();
        const auto files = mwfl::ReadDroppedFiles(reinterpret_cast<HDROP>(event.wparam));
        if (!files.empty()) {
            path_.SetText(files.front());
            AppendActivity(L"Received " + std::to_wstring(files.size()) + L" dropped file(s).");
        }
        return mwfl::EventResult::Handled();
    }

    mwfl::EventResult OnClose() override {
        if (!IsShowcase()) {
            mwfl::SavedWindowPlacement saved;
            if (mwfl::CaptureWindowPlacement(GetHwnd(), saved))
                mwfl::SaveWindowPlacementToRegistry(HKEY_CURRENT_USER, kRegistryKey, L"Window",
                                                     saved);
        }
        return mwfl::EventResult::Propagate();
    }
    mwfl::EventResult OnDpiChanged(const mwfl::DpiChangedEvent& event) override {
        ApplyFont(event.dpi_x);
        return mwfl::EventResult::Propagate();
    }

   private:
    static bool IsShowcase() noexcept {
        return std::wstring_view{::GetCommandLineW()}.find(L"--showcase") !=
               std::wstring_view::npos;
    }

    void ShowCustomDialog() {
        mwfl::Label prompt;
        mwfl::TextBox name;
        mwfl::Button accept, cancel;
        std::wstring accepted_name;
        mwfl::Dialog* dialog_pointer = nullptr;
        mwfl::Dialog dialog({
            .owner = GetHwnd(),
            .title = L"Custom mwfl dialog",
            .initial_client_size = {440.0_dip, 180.0_dip},
            .callbacks = {
                .initialize =
                    [&](HWND window) {
                        mwfl::ControlHost ui{window};
                        ui.Add(prompt, {101}, L"Name this desktop session", {});
                        ui.Add(name, {102}, L"Native Windows", {});
                        ui.Add(accept, {IDOK}, L"OK", {});
                        ui.Add(cancel, {IDCANCEL}, L"Cancel", {});
                        mwfl::SetAccessibleName(name.GetHwnd(), L"Desktop session name");
                        mwfl::SetDialogDefaultButton(window, IDOK);
                        return dialog_pointer->SetLayout(
                            mwfl::Column()
                                .Margin(16.0_dip)
                                .Gap(10.0_dip)
                                .Add(prompt, mwfl::Fixed(24.0_dip))
                                .Add(name, mwfl::Fixed(34.0_dip))
                                .Add(mwfl::Row()
                                         .Gap(8.0_dip)
                                         .Add(mwfl::Column(), mwfl::Stretch())
                                         .Add(accept, mwfl::Fixed(110.0_dip))
                                         .Add(cancel, mwfl::Fixed(110.0_dip)),
                                     mwfl::Fixed(36.0_dip)));
                    },
                .command =
                    [&](HWND, WORD id, WORD) {
                        if (id == IDOK) accepted_name = name.GetText();
                        return false;
                    },
            },
        });
        dialog_pointer = &dialog;
        const mwfl::DialogResult result = dialog.ShowModal();
        if (result) {
            path_.SetText(accepted_name);
            AppendActivity(L"Custom dialog accepted.");
        } else if (result.status == mwfl::DialogStatus::cancelled) {
            AppendActivity(L"Custom dialog cancelled.");
        } else {
            AppendActivity(L"Custom dialog failed with error " + std::to_wstring(result.error) +
                           L".");
        }
    }

    void ShowResult(std::wstring_view action, const mwfl::FileDialogResult& result) {
        if (result.accepted) {
            path_.SetText(result.path.wstring());
            AppendActivity(std::wstring(action) + L": selected a path.");
        } else
            AppendActivity(std::wstring(action) +
                           (result.Cancelled() ? L": cancelled." : L": failed."));
    }
    void AppendActivity(std::wstring_view line) {
        activity_.SetText(activity_.GetText() + L"\r\n" + std::wstring(line));
    }
    void ApplyFont(UINT dpi) {
        if (!font_.CreateMessageFont(dpi)) return;
        for (HWND child = ::GetWindow(GetHwnd(), GW_CHILD); child;
             child = ::GetWindow(child, GW_HWNDNEXT))
            ::SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(font_.GetHandle()), TRUE);
    }
    static constexpr wchar_t kRegistryKey[] = L"Software\\mwfl\\Examples\\DesktopIntegration";
    mwfl::Label title_, subtitle_, path_label_, activity_label_;
    mwfl::GroupBox dialogs_, clipboard_, drop_zone_;
    mwfl::Button open_, save_, folder_, copy_, paste_, task_, custom_;
    mwfl::TextBox path_, activity_;
    mwfl::UiFont font_;
};

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    return mwfl::RunApplication<DesktopIntegrationWindow>(
        instance, show,
        {.title = L"Desktop integration",
         .initial_bounds = {{24.0_dip, 24.0_dip}, {1437.0_dip, 868.5_dip}},
         .use_default_bounds = false});
}
