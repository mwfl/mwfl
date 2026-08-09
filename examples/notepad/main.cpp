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
constexpr mwtl::ControlId kFind{715};
constexpr mwtl::ControlId kReplace{716};
constexpr mwtl::ControlId kRedo{717};
constexpr mwtl::ControlId kRecentBase{730};
constexpr std::wstring_view kSettingsKey = L"Software\\mwtl\\Notepad\\1";

class NotepadWindow final : public mwtl::WindowBase {
public:
    NotepadWindow(mwtl::SingleInstance& instance,
                  std::optional<std::filesystem::path> initial_path)
        : instance_(instance), initial_path_(std::move(initial_path)) {}

    void BuildUI() override {
        if (const auto loaded = mwtl::LoadRecentFilesFromRegistry(
                HKEY_CURRENT_USER, kSettingsKey, recent_.GetMaximumEntries()); loaded.Succeeded()) {
            recent_ = std::move(*loaded.value);
        }
        BuildCommands();
        RefreshRecentCommands();
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
        mwtl::Must(instance_.RegisterWindow(GetHwnd()), "register Notepad activation window");
        SetLayout(mwtl::Column()
            .Add(toolbar_, mwtl::Fixed(36.0_dip))
            .Add(editor_, mwtl::Stretch())
            .Add(status_, mwtl::Fixed(24.0_dip)));
        editor_.Focus();
        mwtl::SavedWindowPlacement placement;
        if (mwtl::LoadWindowPlacementFromRegistry(
                HKEY_CURRENT_USER, kSettingsKey, L"WindowPlacement", placement))
            mwtl::RestoreWindowPlacement(GetHwnd(), placement);
        SyncPresentation(L"Ready");
        if (initial_path_) OpenPath(*initial_path_);
    }

    mwtl::EventResult OnCommand(const mwtl::CommandEvent& event) override {
        if (event.Is(editor_, EN_CHANGE) && !updating_editor_) {
            history_.Record(editor_.GetText());
            SyncDirtyFromHistory();
            SyncPresentation(L"Modified");
            return mwtl::EventResult::Handled();
        }
        return commands_.Dispatch(event);
    }

    mwtl::EventResult OnMessage(const mwtl::WindowMessage& event) override {
        if (auto activation = instance_.DecodeActivation(event.id, event.lparam)) {
            if (::IsIconic(GetHwnd())) ::ShowWindow(GetHwnd(), SW_RESTORE);
            ::SetForegroundWindow(GetHwnd());
            if (!activation->empty() && ConfirmTransition()) OpenPath(*activation);
            return mwtl::EventResult::Handled();
        }
        if (event.id == find_message_) {
            HandleFindReplace(*reinterpret_cast<FINDREPLACEW*>(event.lparam));
            return mwtl::EventResult::Handled();
        }
        if (event.id != WM_DROPFILES) return mwtl::EventResult::Propagate();
        const auto files = mwtl::ReadDroppedFiles(reinterpret_cast<HDROP>(event.wparam));
        if (!files.empty() && ConfirmTransition()) OpenPath(files.front());
        return mwtl::EventResult::Handled();
    }

    mwtl::EventResult OnClose() override {
        if (!ConfirmTransition()) return mwtl::EventResult::Handled();
        mwtl::SavedWindowPlacement placement;
        if (mwtl::CaptureWindowPlacement(GetHwnd(), placement))
            mwtl::SaveWindowPlacementToRegistry(
                HKEY_CURRENT_USER, kSettingsKey, L"WindowPlacement", placement);
        return mwtl::EventResult::Propagate();
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
            .Add(mwtl::Command(kUndo, L"Undo", [this] { ApplyHistory(history_.Undo(), L"Undo"); })
                .SetShortcut({FVIRTKEY | FCONTROL, 'Z'}))
            .Add(mwtl::Command(kRedo, L"Redo", [this] { ApplyHistory(history_.Redo(), L"Redo"); })
                .SetShortcut({FVIRTKEY | FCONTROL, 'Y'}))
            .Add(mwtl::Command(kCut, L"Cut", [this] { ::SendMessageW(editor_.GetHwnd(), WM_CUT, 0, 0); })
                .SetShortcut({FVIRTKEY | FCONTROL, 'X'}))
            .Add(mwtl::Command(kCopy, L"Copy", [this] { ::SendMessageW(editor_.GetHwnd(), WM_COPY, 0, 0); })
                .SetShortcut({FVIRTKEY | FCONTROL, 'C'}))
            .Add(mwtl::Command(kPaste, L"Paste", [this] { ::SendMessageW(editor_.GetHwnd(), WM_PASTE, 0, 0); })
                .SetShortcut({FVIRTKEY | FCONTROL, 'V'}))
            .Add(mwtl::Command(kSelectAll, L"Select All", [this] { editor_.SelectAll(); })
                .SetShortcut({FVIRTKEY | FCONTROL, 'A'}))
            .Add(mwtl::Command(kFind, L"Find...", [this] { ShowFindDialog(false); })
                .SetShortcut({FVIRTKEY | FCONTROL, 'F'}))
            .Add(mwtl::Command(kReplace, L"Replace...", [this] { ShowFindDialog(true); })
                .SetShortcut({FVIRTKEY | FCONTROL, 'H'}));
        for (std::size_t index = 0; index < recent_.GetMaximumEntries(); ++index) {
            commands_.Add(mwtl::Command(
                {static_cast<WORD>(kRecentBase.value + index)}, L"Recent file",
                [this, index] {
                    const auto paths = recent_.GetPaths();
                    if (index < paths.size() && ConfirmTransition()) OpenPath(paths[index]);
                }).SetEnabled(false));
        }
    }

    void BuildMenu() {
        mwtl::Menu next;
        mwtl::Menu file;
        mwtl::Menu edit;
        mwtl::Must(next.Create(), "create Notepad menu");
        mwtl::Must(file.CreatePopup(), "create File menu");
        for (const auto id : {kNew, kOpen, kSave, kSaveAs})
            mwtl::Must(file.AppendCommand(*commands_.Find(id)), "append File command");
        mwtl::Must(file.AppendSeparator(), "append File separator");
        mwtl::Must(file.AppendCommand(*commands_.Find(kExit)), "append Exit command");
        if (!recent_.GetPaths().empty()) {
            mwtl::Must(file.AppendSeparator(), "append recent separator");
            for (std::size_t index = 0; index < recent_.GetMaximumEntries(); ++index)
                mwtl::Must(file.AppendCommand(*commands_.Find(
                    {static_cast<WORD>(kRecentBase.value + index)})), "append recent file");
        }
        mwtl::Must(edit.CreatePopup(), "create Edit menu");
        for (const auto id : {kUndo, kRedo, kCut, kCopy, kPaste, kSelectAll, kFind, kReplace})
            mwtl::Must(edit.AppendCommand(*commands_.Find(id)), "append Edit command");
        mwtl::Must(next.AppendSubmenu(std::move(file), L"File"), "append File menu");
        mwtl::Must(next.AppendSubmenu(std::move(edit), L"Edit"), "append Edit menu");
        mwtl::Must(next.AttachToWindow(GetHwnd()), "attach Notepad menu");
        menu_ = std::move(next);
    }

    void NewDocument() {
        if (!ConfirmTransition()) return;
        updating_editor_ = true;
        editor_.SetText(L"");
        updating_editor_ = false;
        document_.ResetUntitled();
        history_.Reset();
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
        if (!loaded.Succeeded()) {
            ShowTextFileFailure(L"Could not open the file.", loaded.status, loaded.native_error);
            if (loaded.status == mwtl::TextFileStatus::not_found && recent_.Remove(path)) {
                PersistRecentFiles();
                RefreshRecentCommands();
                BuildMenu();
            }
            return;
        }
        updating_editor_ = true;
        editor_.SetText(loaded.value->text);
        updating_editor_ = false;
        document_.MarkOpened(path);
        history_.Reset(loaded.value->text);
        encoding_ = loaded.value->encoding;
        stamp_ = loaded.value->stamp;
        RememberPath(path);
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
        history_.MarkSaved();
        stamp_ = saved.stamp;
        RememberPath(path);
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

    void ApplyHistory(std::optional<std::wstring_view> value, std::wstring_view action) {
        if (!value) return;
        updating_editor_ = true;
        editor_.SetText(*value);
        updating_editor_ = false;
        SyncDirtyFromHistory();
        SyncPresentation(action);
        editor_.Focus();
    }

    void SyncDirtyFromHistory() {
        if (history_.IsModified()) document_.MarkChanged();
        else document_.MarkSaved();
    }

    static std::wstring MenuPathText(std::size_t index, const std::filesystem::path& path) {
        std::wstring text = L"&" + std::to_wstring(index + 1) + L" ";
        for (const wchar_t character : path.wstring()) {
            text += character;
            if (character == L'&') text += L'&';
        }
        return text;
    }

    void RefreshRecentCommands() {
        const auto paths = recent_.GetPaths();
        for (std::size_t index = 0; index < recent_.GetMaximumEntries(); ++index) {
            auto* command = commands_.Find({static_cast<WORD>(kRecentBase.value + index)});
            command->SetEnabled(index < paths.size()).SetVisible(index < paths.size());
            command->SetText(index < paths.size() ? MenuPathText(index, paths[index]) : L"Recent file");
        }
    }

    void PersistRecentFiles() const {
        static_cast<void>(mwtl::SaveRecentFilesToRegistry(HKEY_CURRENT_USER, kSettingsKey, recent_));
    }

    void RememberPath(const std::filesystem::path& path) {
        if (!recent_.Add(path)) return;
        PersistRecentFiles();
        RefreshRecentCommands();
        BuildMenu();
    }

    void ShowFindDialog(bool replace) {
        if (find_dialog_) {
            ::SetForegroundWindow(find_dialog_);
            return;
        }
        find_replace_ = {};
        find_replace_.lStructSize = sizeof(find_replace_);
        find_replace_.hwndOwner = GetHwnd();
        find_replace_.lpstrFindWhat = find_text_.data();
        find_replace_.wFindWhatLen = static_cast<WORD>(find_text_.size());
        find_replace_.Flags = FR_DOWN;
        if (replace) {
            find_replace_.lpstrReplaceWith = replace_text_.data();
            find_replace_.wReplaceWithLen = static_cast<WORD>(replace_text_.size());
            find_dialog_ = ::ReplaceTextW(&find_replace_);
        } else {
            find_dialog_ = ::FindTextW(&find_replace_);
        }
        if (!find_dialog_) ShowFailure(L"Could not open the Find dialog.", ::CommDlgExtendedError());
    }

    void HandleFindReplace(const FINDREPLACEW& request) {
        if (request.Flags & FR_DIALOGTERM) {
            find_dialog_ = nullptr;
            return;
        }
        if (request.Flags & FR_FINDNEXT) {
            FindNext(request);
        } else if (request.Flags & FR_REPLACE) {
            ReplaceSelection(request);
            FindNext(request);
        } else if (request.Flags & FR_REPLACEALL) {
            ReplaceAll(request);
        }
    }

    void FindNext(const FINDREPLACEW& request) {
        const std::wstring text = editor_.GetText();
        const std::wstring_view query{request.lpstrFindWhat};
        DWORD selection_start{}, selection_end{};
        ::SendMessageW(editor_.GetHwnd(), EM_GETSEL,
                       reinterpret_cast<WPARAM>(&selection_start),
                       reinterpret_cast<LPARAM>(&selection_end));
        const bool down = (request.Flags & FR_DOWN) != 0;
        const mwtl::TextSearchOptions options{
            .match_case = (request.Flags & FR_MATCHCASE) != 0,
            .whole_word = (request.Flags & FR_WHOLEWORD) != 0,
            .backwards = !down};
        const auto match = mwtl::FindTextMatch(
            text, query, down ? selection_end : selection_start, options);
        if (!match) {
            status_.SetPartText(0, L"No further match");
            ::MessageBeep(MB_ICONINFORMATION);
            return;
        }
        ::SendMessageW(editor_.GetHwnd(), EM_SETSEL, *match, *match + query.size());
        ::SendMessageW(editor_.GetHwnd(), EM_SCROLLCARET, 0, 0);
        status_.SetPartText(0, L"Match found");
    }

    void ReplaceSelection(const FINDREPLACEW& request) {
        const std::wstring text = editor_.GetText();
        const std::wstring_view query{request.lpstrFindWhat};
        DWORD start{}, end{};
        ::SendMessageW(editor_.GetHwnd(), EM_GETSEL,
                       reinterpret_cast<WPARAM>(&start), reinterpret_cast<LPARAM>(&end));
        const mwtl::TextSearchOptions options{
            .match_case = (request.Flags & FR_MATCHCASE) != 0,
            .whole_word = (request.Flags & FR_WHOLEWORD) != 0};
        if (end >= start && end - start == query.size() &&
            mwtl::TextMatchesAt(text, query, start, options)) {
            ::SendMessageW(editor_.GetHwnd(), EM_REPLACESEL, TRUE,
                           reinterpret_cast<LPARAM>(request.lpstrReplaceWith));
        }
    }

    void ReplaceAll(const FINDREPLACEW& request) {
        std::wstring text = editor_.GetText();
        const std::wstring query{request.lpstrFindWhat};
        const std::wstring replacement{request.lpstrReplaceWith};
        const std::size_t count = mwtl::ReplaceAllText(text, query, replacement, {
            .match_case = (request.Flags & FR_MATCHCASE) != 0,
            .whole_word = (request.Flags & FR_WHOLEWORD) != 0});
        if (count) editor_.SetText(text);
        status_.SetPartText(0, L"Replaced " + std::to_wstring(count) + L" occurrence(s)");
    }

    void SyncPresentation(std::wstring_view message) {
        auto* save = commands_.Find(kSave);
        auto* undo = commands_.Find(kUndo);
        auto* redo = commands_.Find(kRedo);
        save->SetEnabled(document_.IsDirty());
        undo->SetEnabled(history_.CanUndo());
        redo->SetEnabled(history_.CanRedo());
        toolbar_.UpdateCommand(*save);
        for (const auto* command : {save, undo, redo}) menu_.UpdateCommand(*command);
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
    mwtl::SingleInstance& instance_;
    std::optional<std::filesystem::path> initial_path_;
    mwtl::RecentFileList recent_{5};
    mwtl::TextHistory history_;
    mwtl::TextEncoding encoding_ = mwtl::TextEncoding::utf8;
    std::optional<mwtl::FileStamp> stamp_;
    bool updating_editor_{};
    mwtl::CommandSet commands_;
    mwtl::Menu menu_;
    mwtl::AcceleratorTable accelerators_;
    mwtl::Toolbar toolbar_;
    mwtl::TextBox editor_;
    mwtl::StatusBar status_;
    UINT find_message_ = ::RegisterWindowMessageW(FINDMSGSTRING);
    HWND find_dialog_{};
    FINDREPLACEW find_replace_{};
    std::array<wchar_t, 256> find_text_{};
    std::array<wchar_t, 256> replace_text_{};
};
}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    int argument_count{};
    wchar_t** arguments = ::CommandLineToArgvW(::GetCommandLineW(), &argument_count);
    std::optional<std::filesystem::path> initial_path;
    if (arguments && argument_count > 1) {
        std::error_code error;
        auto absolute = std::filesystem::absolute(arguments[1], error);
        initial_path = error ? std::filesystem::path{arguments[1]} : std::move(absolute);
    }
    if (arguments) ::LocalFree(arguments);

    mwtl::SingleInstance single_instance{L"everettjf.mwtl.notepad.v1"};
    if (!single_instance.IsPrimary()) {
        const std::wstring payload = initial_path ? initial_path->wstring() : std::wstring{};
        const auto activation = single_instance.ForwardActivation(payload);
        if (!activation.Delivered()) {
            ::MessageBoxW(nullptr, L"The existing mwtl Notepad instance did not respond.",
                          L"mwtl Notepad", MB_OK | MB_ICONERROR);
            return 2;
        }
        return 0;
    }
    return mwtl::RunApplication<NotepadWindow>(instance, show,
        {.title = L"mwtl Notepad", .initial_bounds = {{}, {900.0_dip, 640.0_dip}},
         .use_default_bounds = false}, {}, single_instance, std::move(initial_path));
}
