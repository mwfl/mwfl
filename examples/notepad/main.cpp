#include <mwtl/mwtl.h>

#include <array>
#include <optional>
#include <string>

using mwtl::operator""_dip;

namespace {
constexpr mwtl::ControlId kNew{700};
constexpr mwtl::ControlId kOpen{701};
constexpr mwtl::ControlId kSave{702};
constexpr mwtl::ControlId kSaveAs{703};
constexpr mwtl::ControlId kExit{704};
constexpr mwtl::ControlId kUndo{710};
constexpr mwtl::ControlId kCut{711};
constexpr mwtl::ControlId kCopy{712};
constexpr mwtl::ControlId kPaste{713};
constexpr mwtl::ControlId kSelectAll{714};

class NotepadWindow final : public mwtl::WindowBase {
public:
    void BuildUI() override {
        BuildCommands();
        mwtl::ControlHost ui{*this};
        ui.Add(toolbar_);
        mwtl::TextBoxOptions options;
        options.style |= ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL |
                         WS_VSCROLL | WS_HSCROLL | ES_NOHIDESEL;
        ui.Add(editor_, L"", options);
        ui.Add(status_);
        for (const auto& command : commands_.GetCommands()) {
            if (command.GetId().value <= kSave.value) {
                mwtl::Must(toolbar_.AddCommand(command), "add Notepad toolbar command");
            }
        }
        toolbar_.AutoSize();
        constexpr std::array status_parts{-1};
        mwtl::Must(status_.SetParts(status_parts), "configure Notepad status bar");
        BuildMenu();
        mwtl::Must(accelerators_.Create(commands_), "create Notepad accelerators");
        SetAccelerators(accelerators_.GetHandle());
        mwtl::EnableFileDrop(GetHwnd());
        SetLayout(mwtl::Column()
            .Add(toolbar_, mwtl::Fixed(36.0_dip))
            .Add(editor_, mwtl::Stretch())
            .Add(status_, mwtl::Fixed(24.0_dip)));
        editor_.Focus();
        SyncPresentation(L"Ready");
    }

    mwtl::EventResult OnCommand(const mwtl::CommandEvent& event) override {
        if (event.Is(editor_, EN_CHANGE) && !updating_editor_) {
            document_.MarkChanged();
            SyncPresentation(L"Modified");
            return mwtl::EventResult::Handled();
        }
        return commands_.Dispatch(event);
    }

    mwtl::EventResult OnMessage(const mwtl::WindowMessage& event) override {
        if (event.id != WM_DROPFILES) return mwtl::EventResult::Propagate();
        const auto files = mwtl::ReadDroppedFiles(reinterpret_cast<HDROP>(event.wparam));
        if (!files.empty() && ConfirmTransition()) OpenPath(files.front());
        return mwtl::EventResult::Handled();
    }

    mwtl::EventResult OnClose() override {
        return ConfirmTransition() ? mwtl::EventResult::Propagate()
                                   : mwtl::EventResult::Handled();
    }

private:
    void BuildCommands() {
        commands_
            .Add(mwtl::Command(kNew, L"New", [this] { NewDocument(); })
                .SetShortcut({FVIRTKEY | FCONTROL, 'N'}))
            .Add(mwtl::Command(kOpen, L"Open...", [this] { OpenDocument(); })
                .SetShortcut({FVIRTKEY | FCONTROL, 'O'}))
            .Add(mwtl::Command(kSave, L"Save", [this] { static_cast<void>(SaveDocument(false)); })
                .SetShortcut({FVIRTKEY | FCONTROL, 'S'}))
            .Add(mwtl::Command(kSaveAs, L"Save As...", [this] { static_cast<void>(SaveDocument(true)); })
                .SetShortcut({FVIRTKEY | FCONTROL | FSHIFT, 'S'}))
            .Add(mwtl::Command(kExit, L"Exit", [this] { static_cast<void>(Close()); }))
            .Add(mwtl::Command(kUndo, L"Undo", [this] { ::SendMessageW(editor_.GetHwnd(), WM_UNDO, 0, 0); })
                .SetShortcut({FVIRTKEY | FCONTROL, 'Z'}))
            .Add(mwtl::Command(kCut, L"Cut", [this] { ::SendMessageW(editor_.GetHwnd(), WM_CUT, 0, 0); })
                .SetShortcut({FVIRTKEY | FCONTROL, 'X'}))
            .Add(mwtl::Command(kCopy, L"Copy", [this] { ::SendMessageW(editor_.GetHwnd(), WM_COPY, 0, 0); })
                .SetShortcut({FVIRTKEY | FCONTROL, 'C'}))
            .Add(mwtl::Command(kPaste, L"Paste", [this] { ::SendMessageW(editor_.GetHwnd(), WM_PASTE, 0, 0); })
                .SetShortcut({FVIRTKEY | FCONTROL, 'V'}))
            .Add(mwtl::Command(kSelectAll, L"Select All", [this] { editor_.SelectAll(); })
                .SetShortcut({FVIRTKEY | FCONTROL, 'A'}));
    }

    void BuildMenu() {
        mwtl::Menu file;
        mwtl::Menu edit;
        mwtl::Must(menu_.Create(), "create Notepad menu");
        mwtl::Must(file.CreatePopup(), "create File menu");
        for (const auto id : {kNew, kOpen, kSave, kSaveAs})
            mwtl::Must(file.AppendCommand(*commands_.Find(id)), "append File command");
        mwtl::Must(file.AppendSeparator(), "append File separator");
        mwtl::Must(file.AppendCommand(*commands_.Find(kExit)), "append Exit command");
        mwtl::Must(edit.CreatePopup(), "create Edit menu");
        for (const auto id : {kUndo, kCut, kCopy, kPaste, kSelectAll})
            mwtl::Must(edit.AppendCommand(*commands_.Find(id)), "append Edit command");
        mwtl::Must(menu_.AppendSubmenu(std::move(file), L"File"), "append File menu");
        mwtl::Must(menu_.AppendSubmenu(std::move(edit), L"Edit"), "append Edit menu");
        mwtl::Must(menu_.AttachToWindow(GetHwnd()), "attach Notepad menu");
    }

    void NewDocument() {
        if (!ConfirmTransition()) return;
        updating_editor_ = true;
        editor_.SetText(L"");
        updating_editor_ = false;
        document_.ResetUntitled();
        encoding_ = mwtl::TextEncoding::utf8;
        stamp_.reset();
        SyncPresentation(L"New document");
        editor_.Focus();
    }

    void OpenDocument() {
        const auto selected = mwtl::ShowOpenFileDialog({
            .owner = GetHwnd(), .title = L"Open text file",
            .filters = {{L"Text files", L"*.txt;*.log;*.md;*.cpp;*.h"}, {L"All files", L"*.*"}}});
        if (selected.Cancelled()) return;
        if (!selected.accepted) { ShowFailure(L"The Open dialog failed.", selected.extended_error); return; }
        if (!ConfirmTransition()) return;
        OpenPath(selected.path);
    }

    void OpenPath(const std::filesystem::path& path) {
        const auto loaded = mwtl::ReadTextFile(path);
        if (!loaded.Succeeded()) { ShowTextFileFailure(L"Could not open the file.", loaded.status, loaded.native_error); return; }
        updating_editor_ = true;
        editor_.SetText(loaded.value->text);
        updating_editor_ = false;
        document_.MarkOpened(path);
        encoding_ = loaded.value->encoding;
        stamp_ = loaded.value->stamp;
        SyncPresentation(L"Opened " + path.filename().wstring());
        editor_.Focus();
    }

    bool SaveDocument(bool choose_path) {
        auto path = document_.GetPath();
        if (choose_path || !document_.HasPath()) {
            const auto selected = mwtl::ShowSaveFileDialog({
                .owner = GetHwnd(), .title = L"Save text file",
                .filters = {{L"Text files", L"*.txt"}, {L"All files", L"*.*"}},
                .default_extension = L"txt", .initial_path = path});
            if (selected.Cancelled()) return false;
            if (!selected.accepted) { ShowFailure(L"The Save dialog failed.", selected.extended_error); return false; }
            path = selected.path;
        }
        const bool same_path = document_.HasPath() && path == document_.GetPath();
        const auto saved = mwtl::WriteTextFileAtomic(path, editor_.GetText(), encoding_,
                                                     same_path ? stamp_ : std::nullopt);
        if (!saved.Succeeded()) { ShowTextFileFailure(L"Could not save the file.", saved.status, saved.native_error); return false; }
        document_.MarkSavedAs(path);
        stamp_ = saved.stamp;
        SyncPresentation(L"Saved " + path.filename().wstring());
        return true;
    }

    bool ConfirmTransition() {
        if (document_.EvaluateTransition() == mwtl::DocumentTransition::proceed) return true;
        const std::wstring message = L"Save changes to " + document_.GetDisplayName() + L"?";
        const int answer = ::MessageBoxW(GetHwnd(), message.c_str(), L"mwtl Notepad",
                                         MB_ICONWARNING | MB_YESNOCANCEL | MB_DEFBUTTON1);
        if (answer == IDCANCEL) return false;
        if (answer == IDNO) return document_.EvaluateTransition(
            mwtl::UnsavedChangesChoice::discard) == mwtl::DocumentTransition::proceed;
        return answer == IDYES && SaveDocument(false);
    }

    void SyncPresentation(std::wstring message) {
        auto* save = commands_.Find(kSave);
        save->SetEnabled(document_.IsDirty());
        toolbar_.UpdateCommand(*save);
        menu_.UpdateCommand(*save);
        std::wstring title = document_.GetDisplayName();
        if (document_.IsDirty()) title += L" *";
        title += L" — mwtl Notepad";
        SetTitle(title);
        status_.SetPartText(0, message);
        ::DrawMenuBar(GetHwnd());
    }

    void ShowFailure(std::wstring_view message, DWORD error) const {
        std::wstring detail{message};
        if (error) detail += L"\n\nWindows error: " + std::to_wstring(error);
        ::MessageBoxW(GetHwnd(), detail.c_str(), L"mwtl Notepad", MB_OK | MB_ICONERROR);
    }

    void ShowTextFileFailure(std::wstring_view message, mwtl::TextFileStatus status, DWORD error) const {
        std::wstring detail{message};
        if (status == mwtl::TextFileStatus::changed)
            detail += L"\n\nThe file changed outside the editor. Your unsaved text was preserved.";
        else if (status == mwtl::TextFileStatus::invalid_encoding)
            detail += L"\n\nThe file contains malformed UTF-8 or UTF-16 data.";
        ShowFailure(detail, error);
    }

    mwtl::DocumentState document_;
    mwtl::TextEncoding encoding_ = mwtl::TextEncoding::utf8;
    std::optional<mwtl::FileStamp> stamp_;
    bool updating_editor_{};
    mwtl::CommandSet commands_;
    mwtl::Menu menu_;
    mwtl::AcceleratorTable accelerators_;
    mwtl::Toolbar toolbar_;
    mwtl::TextBox editor_;
    mwtl::StatusBar status_;
};
}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    return mwtl::RunApplication<NotepadWindow>(instance, show,
        {.title = L"mwtl Notepad", .initial_bounds = {{}, {900.0_dip, 640.0_dip}},
         .use_default_bounds = false});
}
