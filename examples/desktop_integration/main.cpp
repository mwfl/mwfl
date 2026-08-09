#include <mwtl/mwtl.h>

#include <filesystem>
#include <string>

using mwtl::operator""_dip;

namespace {

class DesktopIntegrationWindow final : public mwtl::WindowBase {
   public:
    void BuildUI() override {
        SetTitle(L"Desktop integration toolbox");
        mwtl::ControlHost ui{*this};
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
        mwtl::TextBoxOptions log_options;
        log_options.style |= ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL;
        ui.Add(activity_, L"Ready. Your window position will be restored next time.", log_options);
        activity_.SetReadOnly(true);

        mwtl::EnableFileDrop(GetHwnd());
        mwtl::ApplyWindowAppearance(GetHwnd(), {mwtl::ColorMode::system, mwtl::Backdrop::mica});
        mwtl::SetDialogDefaultButton(GetHwnd(), static_cast<UINT>(open_.GetId().value));
        ApplyFont(GetDpiContext().GetDpi());
        SetLayout(mwtl::Column()
                      .Margin(24.0_dip)
                      .Gap(10.0_dip)
                      .Add(title_, mwtl::Fixed(34.0_dip))
                      .Add(subtitle_, mwtl::Fixed(24.0_dip))
                      .Add(dialogs_, mwtl::Fixed(28.0_dip))
                      .Add(mwtl::Row()
                               .Gap(8.0_dip)
                               .Add(open_, mwtl::Fixed(240.0_dip))
                               .Add(save_, mwtl::Fixed(240.0_dip))
                               .Add(folder_, mwtl::Fixed(240.0_dip))
                               .Add(mwtl::Column(), mwtl::Stretch()),
                           mwtl::Fixed(44.0_dip))
                      .Add(clipboard_, mwtl::Fixed(28.0_dip))
                      .Add(mwtl::Row()
                               .Gap(8.0_dip)
                               .Add(copy_, mwtl::Fixed(210.0_dip))
                               .Add(paste_, mwtl::Fixed(210.0_dip))
                               .Add(task_, mwtl::Fixed(210.0_dip))
                               .Add(custom_, mwtl::Fixed(210.0_dip))
                               .Add(mwtl::Column(), mwtl::Stretch()),
                           mwtl::Fixed(44.0_dip))
                      .Add(drop_zone_, mwtl::Fixed(76.0_dip))
                      .Add(path_label_, mwtl::Auto())
                      .Add(path_, mwtl::Fixed(34.0_dip))
                      .Add(activity_label_, mwtl::Auto())
                      .Add(activity_, mwtl::Stretch()));

        mwtl::SavedWindowPlacement saved;
        if (mwtl::LoadWindowPlacementFromRegistry(HKEY_CURRENT_USER, kRegistryKey, L"Window",
                                                  saved))
            mwtl::RestoreWindowPlacement(GetHwnd(), saved);
    }

    mwtl::EventResult OnCommand(const mwtl::CommandEvent& event) override {
        if (event.IsClicked(open_)) {
            mwtl::FileDialogOptions options{GetHwnd(), L"Open a file with mwtl"};
            options.filters = {{L"Text files", L"*.txt;*.md"}, {L"All files", L"*.*"}};
            ShowResult(L"Open", mwtl::ShowOpenFileDialog(options));
            return mwtl::EventResult::Handled();
        }
        if (event.IsClicked(save_)) {
            mwtl::FileDialogOptions options{GetHwnd(), L"Choose a destination"};
            options.filters = {{L"Text document", L"*.txt"}, {L"All files", L"*.*"}};
            options.default_extension = L"txt";
            ShowResult(L"Save", mwtl::ShowSaveFileDialog(options));
            return mwtl::EventResult::Handled();
        }
        if (event.IsClicked(folder_)) {
            ShowResult(L"Folder",
                       mwtl::ShowFolderDialog({GetHwnd(), L"Choose a workspace folder"}));
            return mwtl::EventResult::Handled();
        }
        if (event.IsClicked(copy_)) {
            const auto value = path_.GetText();
            AppendActivity(mwtl::SetClipboardText(GetHwnd(), value) ? L"Copied the latest result."
                                                                    : L"Clipboard copy failed.");
            return mwtl::EventResult::Handled();
        }
        if (event.IsClicked(paste_)) {
            const auto text = mwtl::GetClipboardText(GetHwnd());
            path_.SetText(text.empty() ? L"Clipboard contains no Unicode text" : text);
            AppendActivity(L"Read Unicode text from the clipboard.");
            return mwtl::EventResult::Handled();
        }
        if (event.IsClicked(task_)) {
            mwtl::ShowTaskDialog(GetHwnd(), L"mwtl desktop integration",
                                 L"Native Windows shell features",
                                 L"These helpers preserve explicit ownership and report "
                                 L"cancellation separately from failure.");
            AppendActivity(L"Displayed a native task dialog.");
            return mwtl::EventResult::Handled();
        }
        if (event.IsClicked(custom_)) {
            ShowCustomDialog();
            return mwtl::EventResult::Handled();
        }
        return mwtl::EventResult::Propagate();
    }

    mwtl::EventResult OnMessage(const mwtl::WindowMessage& event) override {
        if (event.id != WM_DROPFILES) return mwtl::EventResult::Propagate();
        const auto files = mwtl::ReadDroppedFiles(reinterpret_cast<HDROP>(event.wparam));
        if (!files.empty()) {
            path_.SetText(files.front());
            AppendActivity(L"Received " + std::to_wstring(files.size()) + L" dropped file(s).");
        }
        return mwtl::EventResult::Handled();
    }

    mwtl::EventResult OnClose() override {
        mwtl::SavedWindowPlacement saved;
        if (mwtl::CaptureWindowPlacement(GetHwnd(), saved))
            mwtl::SaveWindowPlacementToRegistry(HKEY_CURRENT_USER, kRegistryKey, L"Window", saved);
        return mwtl::EventResult::Propagate();
    }
    mwtl::EventResult OnDpiChanged(const mwtl::DpiChangedEvent& event) override {
        ApplyFont(event.dpi_x);
        return mwtl::EventResult::Propagate();
    }

   private:
    void ShowCustomDialog() {
        mwtl::Label prompt;
        mwtl::TextBox name;
        mwtl::Button accept, cancel;
        std::wstring accepted_name;
        mwtl::Dialog* dialog_pointer = nullptr;
        mwtl::Dialog dialog({
            .owner = GetHwnd(),
            .title = L"Custom mwtl dialog",
            .initial_client_size = {440.0_dip, 180.0_dip},
            .callbacks = {
                .initialize =
                    [&](HWND window) {
                        mwtl::ControlHost ui{window};
                        ui.Add(prompt, {101}, L"Name this desktop session", {});
                        ui.Add(name, {102}, L"Native Windows", {});
                        ui.Add(accept, {IDOK}, L"OK", {});
                        ui.Add(cancel, {IDCANCEL}, L"Cancel", {});
                        mwtl::SetAccessibleName(name.GetHwnd(), L"Desktop session name");
                        mwtl::SetDialogDefaultButton(window, IDOK);
                        return dialog_pointer->SetLayout(
                            mwtl::Column()
                                .Margin(16.0_dip)
                                .Gap(10.0_dip)
                                .Add(prompt, mwtl::Fixed(24.0_dip))
                                .Add(name, mwtl::Fixed(34.0_dip))
                                .Add(mwtl::Row()
                                         .Gap(8.0_dip)
                                         .Add(mwtl::Column(), mwtl::Stretch())
                                         .Add(accept, mwtl::Fixed(110.0_dip))
                                         .Add(cancel, mwtl::Fixed(110.0_dip)),
                                     mwtl::Fixed(36.0_dip)));
                    },
                .command =
                    [&](HWND, WORD id, WORD) {
                        if (id == IDOK) accepted_name = name.GetText();
                        return false;
                    },
            },
        });
        dialog_pointer = &dialog;
        const mwtl::DialogResult result = dialog.ShowModal();
        if (result) {
            path_.SetText(accepted_name);
            AppendActivity(L"Custom dialog accepted.");
        } else if (result.status == mwtl::DialogStatus::cancelled) {
            AppendActivity(L"Custom dialog cancelled.");
        } else {
            AppendActivity(L"Custom dialog failed with error " + std::to_wstring(result.error) +
                           L".");
        }
    }

    void ShowResult(std::wstring_view action, const mwtl::FileDialogResult& result) {
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
    static constexpr wchar_t kRegistryKey[] = L"Software\\mwtl\\Examples\\DesktopIntegration";
    mwtl::Label title_, subtitle_, path_label_, activity_label_;
    mwtl::GroupBox dialogs_, clipboard_, drop_zone_;
    mwtl::Button open_, save_, folder_, copy_, paste_, task_, custom_;
    mwtl::TextBox path_, activity_;
    mwtl::UiFont font_;
};

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    return mwtl::RunApplication<DesktopIntegrationWindow>(
        instance, show,
        {.title = L"Desktop integration",
         .initial_bounds = {{}, {1120.0_dip, 720.0_dip}},
         .use_default_bounds = false});
}
