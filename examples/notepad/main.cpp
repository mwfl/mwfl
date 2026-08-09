#include <mwtl/mwtl.h>

#include <oleacc.h>

#include <array>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

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
constexpr UINT kRunSelfTest = WM_APP + 0x120;

class NotepadWindow final : public mwtl::WindowBase {
public:
    NotepadWindow(mwtl::SingleInstance& instance,
                  std::optional<std::filesystem::path> initial_path,
                  bool self_test,
                  std::optional<std::filesystem::path> self_test_result)
        : instance_(instance), initial_path_(std::move(initial_path)),
          self_test_(self_test), self_test_result_(std::move(self_test_result)) {}

    void BuildUI() override {
        if (!self_test_) {
            const auto loaded = mwtl::LoadRecentFilesFromRegistry(
                HKEY_CURRENT_USER, kSettingsKey, recent_.GetMaximumEntries());
            if (loaded.Succeeded()) recent_ = std::move(*loaded.value);
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
        mwtl::Must(mwtl::SetAccessibleName(toolbar_.GetHwnd(), L"Document commands"),
                   "name Notepad toolbar for accessibility");
        mwtl::Must(mwtl::SetAccessibleName(editor_.GetHwnd(), L"Document text"),
                   "name Notepad editor for accessibility");
        mwtl::Must(mwtl::SetAccessibleName(status_.GetHwnd(), L"Document status"),
                   "name Notepad status for accessibility");
        ApplyFont(GetDpiContext().GetDpi());
        BuildMenu();
        mwtl::Must(accelerators_.Create(commands_), "create Notepad accelerators");
        SetAccelerators(accelerators_.GetHandle());
        mwtl::EnableFileDrop(GetHwnd());
        mwtl::Must(instance_.RegisterWindow(GetHwnd()), "register Notepad activation window");
        SetLayout(mwtl::Column()
            .Add(toolbar_, mwtl::Auto())
            .Add(editor_, mwtl::Stretch())
            .Add(status_, mwtl::Auto()));
        editor_.Focus();
        mwtl::SavedWindowPlacement placement;
        if (!self_test_ && mwtl::LoadWindowPlacementFromRegistry(
                HKEY_CURRENT_USER, kSettingsKey, L"WindowPlacement", placement))
            mwtl::RestoreWindowPlacement(GetHwnd(), placement);
        SyncPresentation(L"Ready");
        if (initial_path_) OpenPath(*initial_path_);
        if (self_test_ && ::PostMessageW(GetHwnd(), kRunSelfTest, 0, 0) == FALSE)
            throw std::runtime_error("post Notepad self-test message failed");
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
        if (event.id == kRunSelfTest) {
            try {
                RunSelfTest();
            } catch (const std::exception& error) {
                ReportSelfTest(error.what());
                ::PostQuitMessage(1);
            } catch (...) {
                ReportSelfTest("unknown Notepad self-test failure");
                ::PostQuitMessage(1);
            }
            return mwtl::EventResult::Handled();
        }
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
        if (event.id == WM_SETTINGCHANGE || event.id == WM_THEMECHANGED) {
            ApplyFont(GetDpiContext().GetDpi());
            ::RedrawWindow(GetHwnd(), nullptr, nullptr,
                           RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
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
        if (!self_test_ && mwtl::CaptureWindowPlacement(GetHwnd(), placement))
            mwtl::SaveWindowPlacementToRegistry(
                HKEY_CURRENT_USER, kSettingsKey, L"WindowPlacement", placement);
        return mwtl::EventResult::Propagate();
    }

    mwtl::EventResult OnDpiChanged(
            const mwtl::DpiChangedEvent& event) override {
        ApplyFont(event.dpi_x);
        return mwtl::EventResult::Propagate();
    }

private:
    [[noreturn]] static void FailSelfTest(const char* message) {
        throw std::runtime_error(message);
    }

    void ReportSelfTest(std::string_view result) const noexcept {
        if (!self_test_result_) return;
        try {
            std::ofstream output{*self_test_result_, std::ios::trunc};
            output << result;
        } catch (...) {
        }
    }

    static bool HasAccessibleName(HWND window, std::wstring_view expected) noexcept {
        IAccessible* accessible = nullptr;
        const HRESULT opened = ::AccessibleObjectFromWindow(
            window, static_cast<DWORD>(OBJID_CLIENT), IID_IAccessible,
            reinterpret_cast<void**>(&accessible));
        if (FAILED(opened) || accessible == nullptr) return false;
        VARIANT self{};
        self.vt = VT_I4;
        self.lVal = static_cast<LONG>(CHILDID_SELF);
        BSTR name = nullptr;
        const HRESULT read = accessible->get_accName(self, &name);
        const bool matches = SUCCEEDED(read) && name != nullptr &&
            std::wstring_view{name, ::SysStringLen(name)} == expected;
        if (name != nullptr) ::SysFreeString(name);
        accessible->Release();
        return matches;
    }

    void RunSelfTest() {
        wchar_t temporary_directory[MAX_PATH + 1]{};
        if (::GetTempPathW(MAX_PATH, temporary_directory) == 0)
            FailSelfTest("resolve Notepad self-test temporary directory failed");
        const std::filesystem::path path =
            std::filesystem::path{temporary_directory} /
            (L"mwtl-notepad-gui-" + std::to_wstring(::GetCurrentProcessId()) + L".txt");
        struct RemoveTemporaryFile {
            std::filesystem::path path;
            ~RemoveTemporaryFile() { std::error_code ignored; std::filesystem::remove(path, ignored); }
        } cleanup{path};

        const auto created = mwtl::WriteTextFileAtomic(
            path, L"initial", mwtl::TextEncoding::utf8);
        if (!created.Succeeded()) FailSelfTest("create Notepad self-test file failed");
        OpenPath(path);
        if (editor_.GetText() != L"initial" || document_.GetPath() != path ||
            document_.IsDirty()) {
            FailSelfTest("Notepad self-test open state mismatch");
        }
        if (!HasAccessibleName(toolbar_.GetHwnd(), L"Document commands") ||
            !HasAccessibleName(editor_.GetHwnd(), L"Document text") ||
            !HasAccessibleName(status_.GetHwnd(), L"Document status")) {
            FailSelfTest("Notepad self-test accessible name mismatch");
        }
        ::SendMessageW(GetHwnd(), WM_THEMECHANGED, 0, 0);
        if (!toolbar_.IsWindow() || !editor_.IsWindow() || !status_.IsWindow())
            FailSelfTest("Notepad self-test theme refresh destroyed a control");
        RECT toolbar_bounds{};
        RECT editor_bounds{};
        RECT status_bounds{};
        if (::GetWindowRect(toolbar_.GetHwnd(), &toolbar_bounds) == FALSE ||
            ::GetWindowRect(editor_.GetHwnd(), &editor_bounds) == FALSE ||
            ::GetWindowRect(status_.GetHwnd(), &status_bounds) == FALSE ||
            toolbar_bounds.bottom <= toolbar_bounds.top ||
            status_bounds.bottom <= status_bounds.top ||
            toolbar_bounds.bottom > editor_bounds.top ||
            editor_bounds.bottom > status_bounds.top) {
            FailSelfTest("Notepad self-test auto layout bounds mismatch");
        }

        editor_.SelectAll();
        ::SendMessageW(editor_.GetHwnd(), EM_REPLACESEL, TRUE,
                       reinterpret_cast<LPARAM>(L"edited"));
        if (!document_.IsDirty()) FailSelfTest("Notepad self-test edit stayed clean");
        if (!SaveDocument(false)) FailSelfTest("Notepad self-test save failed");
        const auto saved = mwtl::ReadTextFile(path);
        if (!saved.Succeeded() || saved.value->text != L"edited" || document_.IsDirty())
            FailSelfTest("Notepad self-test saved content mismatch");

        editor_.SelectAll();
        ::SendMessageW(editor_.GetHwnd(), EM_REPLACESEL, TRUE,
                       reinterpret_cast<LPARAM>(L"unsaved"));
        automated_choice_ = mwtl::UnsavedChangesChoice::cancel;
        NewDocument();
        if (editor_.GetText() != L"unsaved" || document_.GetPath() != path ||
            !document_.IsDirty()) {
            FailSelfTest("Notepad self-test cancel discarded content");
        }

        automated_choice_ = mwtl::UnsavedChangesChoice::discard;
        NewDocument();
        if (!editor_.GetText().empty() || document_.HasPath() || document_.IsDirty())
            FailSelfTest("Notepad self-test discard did not create a clean document");
        OpenPath(path);
        if (editor_.GetText() != L"edited" || document_.IsDirty())
            FailSelfTest("Notepad self-test reopen mismatch");

        const DWORD resources_before =
            ::GetGuiResources(::GetCurrentProcess(), GR_GDIOBJECTS) +
            ::GetGuiResources(::GetCurrentProcess(), GR_USEROBJECTS);
        for (int iteration = 0; iteration < 50; ++iteration) {
            NewDocument();
            OpenPath(path);
            if (editor_.GetText() != L"edited" || document_.IsDirty())
                FailSelfTest("Notepad self-test repeated reopen mismatch");
        }
        const DWORD resources_after =
            ::GetGuiResources(::GetCurrentProcess(), GR_GDIOBJECTS) +
            ::GetGuiResources(::GetCurrentProcess(), GR_USEROBJECTS);
        if (resources_after > resources_before + 4)
            FailSelfTest("Notepad self-test repeated reopen leaked GUI resources");

        LOGFONTW before{};
        LOGFONTW after{};
        if (::GetObjectW(font_.GetHandle(), sizeof(before), &before) == 0)
            FailSelfTest("read Notepad self-test font failed");
        const UINT window_dpi = GetDpiContext().GetDpi();
        const UINT test_dpi = window_dpi == 144 ? 192 : 144;
        ApplyFont(test_dpi);
        if (::GetObjectW(font_.GetHandle(), sizeof(after), &after) == 0 ||
            before.lfHeight == after.lfHeight) {
            throw std::runtime_error("Notepad self-test DPI font did not change: " +
                std::to_string(before.lfHeight) + " -> " +
                std::to_string(after.lfHeight));
        }
        ApplyFont(window_dpi);
        ReportSelfTest("ok");
        if (::PostMessageW(GetHwnd(), WM_CLOSE, 0, 0) == FALSE)
            FailSelfTest("post Notepad self-test close failed");
    }

    void ApplyFont(UINT dpi) {
        mwtl::UiFont next;
        mwtl::Must(next.CreateMessageFont(dpi), "create Notepad UI font");
        for (const HWND control : {toolbar_.GetHwnd(), editor_.GetHwnd(),
                                  status_.GetHwnd()}) {
            mwtl::SetControlFont(control, next.GetHandle());
        }
        font_ = std::move(next);
    }

    void BuildCommands() {
        commands_
            .Add(mwtl::Command(kNew, L"&New", [this] { NewDocument(); })
                .SetShortcut({FVIRTKEY | FCONTROL, 'N'}))
            .Add(mwtl::Command(kOpen, L"&Open...", [this] { OpenDocument(); })
                .SetShortcut({FVIRTKEY | FCONTROL, 'O'}))
            .Add(mwtl::Command(kSave, L"&Save", [this] { static_cast<void>(SaveDocument(false)); })
                .SetShortcut({FVIRTKEY | FCONTROL, 'S'}))
            .Add(mwtl::Command(kSaveAs, L"Save &As...", [this] { static_cast<void>(SaveDocument(true)); })
                .SetShortcut({FVIRTKEY | FCONTROL | FSHIFT, 'S'}))
            .Add(mwtl::Command(kExit, L"E&xit", [this] { static_cast<void>(Close()); }))
            .Add(mwtl::Command(kUndo, L"&Undo", [this] { ApplyHistory(history_.Undo(), L"Undo"); })
                .SetShortcut({FVIRTKEY | FCONTROL, 'Z'}))
            .Add(mwtl::Command(kRedo, L"&Redo", [this] { ApplyHistory(history_.Redo(), L"Redo"); })
                .SetShortcut({FVIRTKEY | FCONTROL, 'Y'}))
            .Add(mwtl::Command(kCut, L"Cu&t", [this] { ::SendMessageW(editor_.GetHwnd(), WM_CUT, 0, 0); })
                .SetShortcut({FVIRTKEY | FCONTROL, 'X'}))
            .Add(mwtl::Command(kCopy, L"&Copy", [this] { ::SendMessageW(editor_.GetHwnd(), WM_COPY, 0, 0); })
                .SetShortcut({FVIRTKEY | FCONTROL, 'C'}))
            .Add(mwtl::Command(kPaste, L"&Paste", [this] { ::SendMessageW(editor_.GetHwnd(), WM_PASTE, 0, 0); })
                .SetShortcut({FVIRTKEY | FCONTROL, 'V'}))
            .Add(mwtl::Command(kSelectAll, L"Select &All", [this] { editor_.SelectAll(); })
                .SetShortcut({FVIRTKEY | FCONTROL, 'A'}))
            .Add(mwtl::Command(kFind, L"&Find...", [this] { ShowFindDialog(false); })
                .SetShortcut({FVIRTKEY | FCONTROL, 'F'}))
            .Add(mwtl::Command(kReplace, L"&Replace...", [this] { ShowFindDialog(true); })
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
        mwtl::Must(next.AppendSubmenu(std::move(file), L"&File"), "append File menu");
        mwtl::Must(next.AppendSubmenu(std::move(edit), L"&Edit"), "append Edit menu");
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
        if (automated_choice_) {
            const auto choice = std::exchange(automated_choice_, std::nullopt);
            const auto transition = document_.EvaluateTransition(*choice);
            if (transition == mwtl::DocumentTransition::proceed) return true;
            if (transition == mwtl::DocumentTransition::cancelled) return false;
            return SaveDocument(false);
        }
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
        if (self_test_) return;
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
    bool self_test_{};
    std::optional<mwtl::UnsavedChangesChoice> automated_choice_;
    std::optional<std::filesystem::path> self_test_result_;
    mwtl::CommandSet commands_;
    mwtl::UiFont font_;
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
    std::optional<std::filesystem::path> self_test_result;
    bool self_test = false;
    for (int index = 1; arguments && index < argument_count; ++index) {
        if (std::wstring_view{arguments[index]} == L"--self-test") {
            self_test = true;
        } else if (std::wstring_view{arguments[index]} == L"--self-test-result" &&
                   index + 1 < argument_count) {
            self_test_result = arguments[++index];
        } else if (!initial_path) {
            std::error_code error;
            auto absolute = std::filesystem::absolute(arguments[index], error);
            initial_path = error ? std::filesystem::path{arguments[index]}
                                 : std::move(absolute);
        }
    }
    if (arguments) ::LocalFree(arguments);

    const std::wstring instance_name = self_test
        ? L"everettjf.mwtl.notepad.self-test." + std::to_wstring(::GetCurrentProcessId())
        : L"everettjf.mwtl.notepad.v1";
    mwtl::SingleInstance single_instance{instance_name};
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
    return mwtl::RunApplication<NotepadWindow>(instance, self_test ? SW_HIDE : show,
        {.title = L"mwtl Notepad", .initial_bounds = {{}, {900.0_dip, 640.0_dip}},
         .use_default_bounds = false}, {}, single_instance, std::move(initial_path),
        self_test, std::move(self_test_result));
}
