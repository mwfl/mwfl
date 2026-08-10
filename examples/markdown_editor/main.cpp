#include <mwtl/mwtl.h>
#include <mwtl/scintilla.h>
#include <mwtl/webview2.h>

#include "markdown_renderer.h"
#include "markdown_syntax.h"
#include "resource.h"

#include <filesystem>
#include <chrono>
#include <cwctype>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

using mwtl::operator""_dip;

namespace {

constexpr mwtl::ControlId kNew{1200};
constexpr mwtl::ControlId kOpen{1201};
constexpr mwtl::ControlId kSave{1202};
constexpr mwtl::ControlId kBold{1203};
constexpr mwtl::ControlId kItalic{1204};
constexpr mwtl::ControlId kCode{1205};
constexpr mwtl::ControlId kHeading{1206};
constexpr mwtl::ControlId kTheme{1207};
constexpr mwtl::ControlId kSaveAs{1208};
constexpr mwtl::ControlId kExit{1209};
constexpr mwtl::ControlId kSplitter{1210};
constexpr mwtl::ControlId kEditor{1211};
constexpr mwtl::ControlId kPreview{1212};
constexpr mwtl::ControlId kUndo{1213};
constexpr mwtl::ControlId kRedo{1214};
constexpr mwtl::ControlId kCut{1215};
constexpr mwtl::ControlId kCopy{1216};
constexpr mwtl::ControlId kPaste{1217};
constexpr mwtl::ControlId kSelectAll{1218};
constexpr mwtl::ControlId kFindFocus{1219};
constexpr mwtl::ControlId kReplaceFocus{1220};
constexpr mwtl::ControlId kFindNext{1221};
constexpr mwtl::ControlId kReplaceNext{1222};
constexpr mwtl::ControlId kSearchText{1223};
constexpr mwtl::ControlId kReplacementText{1224};
constexpr UINT kRunSelfTest = WM_APP + 0x220;
constexpr UINT kFinishSelfTest = WM_APP + 0x221;
constexpr mwtl::TimerId kRecoveryTimer{1225};
constexpr wchar_t kSettingsKey[] = L"Software\\mwtl\\MarkdownEditor";

class MarkdownEditorWindow final : public mwtl::WindowBase {
public:
    void BuildUI() override {
        SetTitle(L"Untitled.md - mwtl Markdown");
        BuildCommands();
        if (!scintilla_runtime_.LoadAdjacent()) {
            throw std::runtime_error(
                "Scintilla.dll is unavailable beside the executable; enable "
                "MWTL_BUILD_SCINTILLA and deploy the runtime");
        }

        mwtl::ControlHost ui{*this};
        ui.Add(new_, kNew, L"New");
        ui.Add(open_, kOpen, L"Open...");
        ui.Add(save_, kSave, L"Save");
        ui.Add(heading_, kHeading, L"H1");
        ui.Add(bold_, kBold, L"Bold");
        ui.Add(italic_, kItalic, L"Italic");
        ui.Add(code_, kCode, L"Code");
        ui.Add(theme_button_, kTheme, L"Dark");
        ui.Add(search_, kSearchText, L"");
        ui.Add(find_next_, kFindNext, L"Find next");
        ui.Add(replacement_, kReplacementText, L"");
        ui.Add(replace_next_, kReplaceNext, L"Replace");
        ui.Add(splitter_, kSplitter, mwtl::RectDip{},
               mwtl::SplitterOptions{.constraints = {260.0_dip, 260.0_dip, 6.0_dip},
                                     .initial_position = 620.0_dip});
        mwtl::ControlHost panes{splitter_};
        panes.AddNative(editor_, kEditor, mwtl::RectDip{}, scintilla_runtime_);
        panes.AddNative(preview_, kPreview, mwtl::RectDip{});
        ui.Add(status_, L"Starting preview...");

        mwtl::Must(splitter_.AttachPanes(editor_.GetHwnd(), preview_.GetHwnd()),
                   "attach Markdown editor panes");
        mwtl::Must(editor_.ConfigureCodeEditing({.font = L"Cascadia Mono",
                                                 .font_size_points = 11.0f,
                                                 .tab_width = 2,
                                                 .use_tabs = false,
                                                 .word_wrap = true,
                                                 .show_line_numbers = true}),
                   "configure Markdown source editor");
        if (!markdown_syntax_.Attach(editor_)) {
            throw std::runtime_error(
                "Lexilla.dll or its Markdown lexer is unavailable beside the executable");
        }
        markdown_syntax_.ApplyTheme(editor_, markdown_editor::EditorTheme::light);
        mwtl::Must(mwtl::SetAccessibleName(editor_.GetHwnd(), L"Markdown source"),
                   "name Markdown source editor");
        mwtl::Must(mwtl::SetAccessibleName(preview_.GetHwnd(), L"Markdown preview"),
                   "name Markdown preview");
        mwtl::Must(mwtl::SetAccessibleName(status_.GetHwnd(), L"Document status"),
                   "name Markdown status");
        mwtl::Must(mwtl::SetAccessibleName(search_.GetHwnd(), L"Find text"),
                   "name Markdown search field");
        mwtl::Must(mwtl::SetAccessibleName(replacement_.GetHwnd(), L"Replacement text"),
                   "name Markdown replacement field");
        BuildMenu();
        mwtl::Must(accelerators_.Create(commands_), "create Markdown accelerator table");
        SetAccelerators(accelerators_.GetHandle());
        mwtl::ApplyWindowAppearance(
            GetHwnd(), {mwtl::ColorMode::system, mwtl::Backdrop::mica});

        SetLayout(mwtl::Column().Gap(6.0_dip).Margin(8.0_dip)
            .Add(mwtl::Row().Gap(6.0_dip)
                .Add(new_, mwtl::Fixed(62.0_dip))
                .Add(open_, mwtl::Fixed(78.0_dip))
                .Add(save_, mwtl::Fixed(68.0_dip))
                .Add(heading_, mwtl::Fixed(56.0_dip))
                .Add(bold_, mwtl::Fixed(62.0_dip))
                .Add(italic_, mwtl::Fixed(62.0_dip))
                .Add(code_, mwtl::Fixed(62.0_dip))
                .Add(theme_button_, mwtl::Fixed(72.0_dip)), mwtl::Fixed(32.0_dip))
            .Add(mwtl::Row().Gap(6.0_dip)
                .Add(search_, mwtl::Stretch())
                .Add(find_next_, mwtl::Fixed(86.0_dip))
                .Add(replacement_, mwtl::Stretch())
                .Add(replace_next_, mwtl::Fixed(82.0_dip)), mwtl::Fixed(30.0_dip))
            .Add(splitter_, mwtl::Stretch())
            .Add(status_, mwtl::Auto()));
        // The splitter is created before retained layout gives it a useful
        // extent, so apply the preferred product ratio after the first arrange.
        mwtl::Must(splitter_.SetPosition(500.0_dip),
                   "set Markdown editor pane width");

        const std::wstring welcome =
            L"# Welcome to mwtl Markdown\n\n"
            L"A small, native Windows Markdown editor built with **mwtl**, "
            L"Scintilla, and WebView2.\n\n"
            L"- Edit Markdown on the left\n"
            L"- See the offline preview on the right\n"
            L"- [x] Keep every document local\n\n"
            L"> Native Windows UI, without the Win32 ceremony.\n\n"
            L"```cpp\n#include <mwtl/mwtl.h>\n```\n";
        mwtl::Must(editor_.SetText(welcome), "set Markdown welcome document");
        editor_.SetSavePoint();
        document_.ResetUntitled();
        InitializeRecovery();
        StartPreview();
        UpdatePresentation(L"Ready");
        editor_.Focus();
        mwtl::SavedWindowPlacement placement;
        if (!IsSelfTest() && mwtl::LoadWindowPlacementFromRegistry(
                HKEY_CURRENT_USER, kSettingsKey, L"WindowPlacement", placement))
            mwtl::RestoreWindowPlacement(GetHwnd(), placement);

        if (IsSelfTest() && ::PostMessageW(GetHwnd(), kRunSelfTest, 0, 0) == FALSE)
            throw std::runtime_error("post Markdown editor self-test failed");
    }

    mwtl::EventResult OnCommand(const mwtl::CommandEvent& event) override {
        return commands_.Dispatch(event);
    }

    mwtl::EventResult OnNotify(const mwtl::NotifyEvent& event) override {
        if (!event.IsFrom(editor_)) return mwtl::EventResult::Propagate();
        const auto notification = editor_.DecodeNotification(event.header);
        if (!notification) return mwtl::EventResult::Propagate();
        if (notification->kind == mwtl::ScintillaNotificationKind::save_point_left) {
            document_.MarkChanged();
            UpdatePresentation(L"Modified");
        } else if (notification->kind == mwtl::ScintillaNotificationKind::save_point_reached) {
            document_.MarkSaved();
            UpdatePresentation(L"Saved");
        } else if (notification->kind == mwtl::ScintillaNotificationKind::modified) {
            if (notification->lines_added != 0)
                static_cast<void>(editor_.UpdateLineNumberMargin());
            RefreshPreview();
            UpdatePresentation(L"Editing");
            ScheduleRecovery();
        }
        return mwtl::EventResult::Propagate();
    }

    mwtl::EventResult OnMessage(const mwtl::WindowMessage& event) override {
        if (event.id == kRunSelfTest) {
            RunSelfTest();
            return mwtl::EventResult::Handled();
        }
        if (event.id == kFinishSelfTest) {
            FinishSelfTest();
            return mwtl::EventResult::Handled();
        }
        return mwtl::EventResult::Propagate();
    }

    mwtl::EventResult OnDpiChanged(const mwtl::DpiChangedEvent&) override {
        static_cast<void>(editor_.ConfigureCodeEditing({.font = L"Cascadia Mono",
                                                        .font_size_points = 11.0f,
                                                        .tab_width = 2,
                                                        .word_wrap = true}));
        markdown_syntax_.ApplyTheme(editor_, theme_ == markdown_editor::PreviewTheme::dark
            ? markdown_editor::EditorTheme::dark : markdown_editor::EditorTheme::light);
        return mwtl::EventResult::Propagate();
    }

    mwtl::EventResult OnTimer(mwtl::TimerId id) override {
        if (id != kRecoveryTimer) return mwtl::EventResult::Propagate();
        recovery_timer_.Stop();
        WriteRecovery();
        return mwtl::EventResult::Handled();
    }

    mwtl::EventResult OnClose() override {
        if (!IsSelfTest() && !ConfirmDiscardOrSave()) return mwtl::EventResult::Handled();
        recovery_timer_.Stop();
        RemoveRecovery();
        if (!IsSelfTest()) {
            mwtl::SavedWindowPlacement placement;
            if (mwtl::CaptureWindowPlacement(GetHwnd(), placement))
                static_cast<void>(mwtl::SaveWindowPlacementToRegistry(
                    HKEY_CURRENT_USER, kSettingsKey, L"WindowPlacement", placement));
        }
        preview_.Close();
        if (IsSelfTest()) {
            std::error_code ignored;
            if (!self_test_path_.empty()) std::filesystem::remove(self_test_path_, ignored);
            if (!preview_data_folder_.empty())
                std::filesystem::remove_all(preview_data_folder_, ignored);
        }
        return mwtl::EventResult::Propagate();
    }

private:
    void BuildCommands() {
        commands_
            .Add(mwtl::Command(kNew, L"&New", [this] { NewDocument(); })
                .SetShortcut({FVIRTKEY | FCONTROL, 'N'}))
            .Add(mwtl::Command(kOpen, L"&Open...", [this] { OpenInteractive(); })
                .SetShortcut({FVIRTKEY | FCONTROL, 'O'}))
            .Add(mwtl::Command(kSave, L"&Save", [this] { static_cast<void>(SaveInteractive()); })
                .SetShortcut({FVIRTKEY | FCONTROL, 'S'}))
            .Add(mwtl::Command(kSaveAs, L"Save &As...", [this] { static_cast<void>(SaveAsInteractive()); })
                .SetShortcut({FVIRTKEY | FCONTROL | FSHIFT, 'S'}))
            .Add(mwtl::Command(kExit, L"E&xit", [this] { static_cast<void>(Close()); }))
            .Add(mwtl::Command(kUndo, L"&Undo", [this] { editor_.Undo(); })
                .SetShortcut({FVIRTKEY | FCONTROL, 'Z'}))
            .Add(mwtl::Command(kRedo, L"&Redo", [this] { editor_.Redo(); })
                .SetShortcut({FVIRTKEY | FCONTROL, 'Y'}))
            .Add(mwtl::Command(kCut, L"Cu&t", [this] { editor_.Cut(); })
                .SetShortcut({FVIRTKEY | FCONTROL, 'X'}))
            .Add(mwtl::Command(kCopy, L"&Copy", [this] { editor_.Copy(); })
                .SetShortcut({FVIRTKEY | FCONTROL, 'C'}))
            .Add(mwtl::Command(kPaste, L"&Paste", [this] { editor_.Paste(); })
                .SetShortcut({FVIRTKEY | FCONTROL, 'V'}))
            .Add(mwtl::Command(kSelectAll, L"Select &All", [this] { editor_.SelectAll(); })
                .SetShortcut({FVIRTKEY | FCONTROL, 'A'}))
            .Add(mwtl::Command(kFindFocus, L"&Find", [this] { search_.Focus(); search_.SelectAll(); })
                .SetShortcut({FVIRTKEY | FCONTROL, 'F'}))
            .Add(mwtl::Command(kReplaceFocus, L"&Replace", [this] { replacement_.Focus(); replacement_.SelectAll(); })
                .SetShortcut({FVIRTKEY | FCONTROL, 'H'}))
            .Add(mwtl::Command(kFindNext, L"Find &next", [this] { static_cast<void>(FindNext()); })
                .SetShortcut({FVIRTKEY, VK_F3}))
            .Add(mwtl::Command(kReplaceNext, L"Replace next", [this] { ReplaceNext(); }))
            .Add(mwtl::Command(kHeading, L"Heading 1", [this] { PrefixSelection(L"# "); }))
            .Add(mwtl::Command(kBold, L"&Bold", [this] { WrapSelection(L"**", L"**", L"bold text"); })
                .SetShortcut({FVIRTKEY | FCONTROL, 'B'}))
            .Add(mwtl::Command(kItalic, L"&Italic", [this] { WrapSelection(L"*", L"*", L"italic text"); })
                .SetShortcut({FVIRTKEY | FCONTROL, 'I'}))
            .Add(mwtl::Command(kCode, L"Inline &code", [this] { WrapSelection(L"`", L"`", L"code"); }))
            .Add(mwtl::Command(kTheme, L"Toggle preview theme", [this] { ToggleTheme(); })
                .SetShortcut({FVIRTKEY | FCONTROL, 'D'}));
    }

    void BuildMenu() {
        mwtl::Menu bar, file, edit, format, view;
        mwtl::Must(bar.Create(), "create Markdown menu bar");
        mwtl::Must(file.CreatePopup(), "create Markdown File menu");
        for (const auto id : {kNew, kOpen, kSave, kSaveAs})
            mwtl::Must(file.AppendCommand(*commands_.Find(id)), "append Markdown File command");
        mwtl::Must(file.AppendSeparator(), "append Markdown File separator");
        mwtl::Must(file.AppendCommand(*commands_.Find(kExit)), "append Markdown Exit command");
        mwtl::Must(edit.CreatePopup(), "create Markdown Edit menu");
        for (const auto id : {kUndo, kRedo, kCut, kCopy, kPaste, kSelectAll, kFindFocus,
                              kReplaceFocus, kFindNext})
            mwtl::Must(edit.AppendCommand(*commands_.Find(id)), "append Markdown Edit command");
        mwtl::Must(format.CreatePopup(), "create Markdown Format menu");
        for (const auto id : {kHeading, kBold, kItalic, kCode})
            mwtl::Must(format.AppendCommand(*commands_.Find(id)), "append Markdown Format command");
        mwtl::Must(view.CreatePopup(), "create Markdown View menu");
        mwtl::Must(view.AppendCommand(*commands_.Find(kTheme)), "append Markdown View command");
        mwtl::Must(bar.AppendSubmenu(std::move(file), L"&File"), "append Markdown File menu");
        mwtl::Must(bar.AppendSubmenu(std::move(edit), L"&Edit"), "append Markdown Edit menu");
        mwtl::Must(bar.AppendSubmenu(std::move(format), L"F&ormat"), "append Markdown Format menu");
        mwtl::Must(bar.AppendSubmenu(std::move(view), L"&View"), "append Markdown View menu");
        mwtl::Must(bar.AttachToWindow(GetHwnd()), "attach Markdown menu");
        menu_ = std::move(bar);
    }

    static bool IsSelfTest() noexcept {
        return std::wstring_view{::GetCommandLineW()}.find(L"--self-test") !=
               std::wstring_view::npos;
    }

    void StartPreview() {
        if (IsSelfTest()) {
            preview_data_folder_ = std::filesystem::temp_directory_path() /
                (L"mwtl-markdown-editor-webview-" +
                 std::to_wstring(::GetCurrentProcessId()));
        }
        const bool started = preview_.Initialize({.user_data_folder = preview_data_folder_}, {
            .initialized = [this](mwtl::WebView2InitializationResult result) {
                if (result.state != mwtl::WebView2HostState::ready) {
                    status_.SetText(result.runtime == mwtl::WebView2RuntimeStatus::missing
                        ? L"WebView2 Runtime is missing; editing and saving remain available"
                        : L"Preview initialization failed; editing and saving remain available");
                    if (IsSelfTest()) ::PostQuitMessage(
                        result.runtime == mwtl::WebView2RuntimeStatus::missing ? 0 : 20);
                    return;
                }
                preview_ready_ = true;
                RefreshPreview();
            },
            .navigation_completed = [this](bool succeeded, HRESULT) {
                preview_navigation_succeeded_ = succeeded;
                if (IsSelfTest() && self_test_started_ &&
                    ::PostMessageW(GetHwnd(), kFinishSelfTest, 0, 0) == FALSE)
                    ::PostQuitMessage(21);
            },
            .process_failed = [this](mwtl::WebView2ProcessFailureKind) {
                preview_ready_ = false;
                status_.SetText(L"Preview process stopped; press the theme button to retry rendering");
            }});
        if (!started) status_.SetText(L"Preview could not start; editor remains available");
    }

    void InitializeRecovery() {
        if (IsSelfTest()) return;
        wchar_t local_data[32768]{};
        const DWORD length = ::GetEnvironmentVariableW(
            L"LOCALAPPDATA", local_data, static_cast<DWORD>(std::size(local_data)));
        if (length == 0 || length >= std::size(local_data)) return;
        recovery_path_ = std::filesystem::path{local_data} / L"mwtl" /
                         L"MarkdownEditor" / L"recovery.md";
        std::error_code error;
        std::filesystem::create_directories(recovery_path_.parent_path(), error);
        if (error || !std::filesystem::exists(recovery_path_, error)) return;
        const auto recovered = mwtl::ReadTextFile(recovery_path_);
        if (!recovered.Succeeded() || recovered.value->text.empty()) return;
        const int answer = ::MessageBoxW(
            GetHwnd(), L"An unsaved document was recovered from the previous session. Restore it?",
            L"mwtl Markdown", MB_YESNO | MB_ICONINFORMATION);
        if (answer == IDYES && editor_.SetText(recovered.value->text)) {
            editor_.SetSavePoint();
            document_.ResetUntitled();
            document_.MarkChanged();
            status_.SetText(L"Recovered unsaved document");
        } else {
            RemoveRecovery();
        }
    }

    void ScheduleRecovery() {
        if (IsSelfTest() || recovery_path_.empty() || recovery_timer_.IsRunning()) return;
        static_cast<void>(recovery_timer_.Start(*this, kRecoveryTimer,
                                                 std::chrono::milliseconds{1500}));
    }

    void WriteRecovery() {
        if (recovery_path_.empty() || !document_.IsDirty()) return;
        const auto text = editor_.GetText();
        if (text) static_cast<void>(mwtl::WriteTextFileAtomic(
            recovery_path_, *text, mwtl::TextEncoding::utf8));
    }

    void RemoveRecovery() noexcept {
        if (recovery_path_.empty()) return;
        std::error_code ignored;
        std::filesystem::remove(recovery_path_, ignored);
    }

    void RefreshPreview() {
        const auto text = editor_.GetText();
        if (!text) return;
        markdown_editor::RenderOptions options{.theme = theme_};
        if (document_.HasPath()) options.document_directory = document_.GetPath().parent_path();
        pending_html_ = markdown_editor::RenderMarkdown(*text, options);
        if (preview_ready_ && !preview_.NavigateToString(pending_html_))
            status_.SetText(L"Preview update failed; document text is safe");
    }

    bool OpenPath(const std::filesystem::path& path) {
        const auto loaded = mwtl::ReadTextFile(path);
        if (!loaded.Succeeded() || !editor_.SetText(loaded.value->text)) {
            ::MessageBoxW(GetHwnd(), L"The Markdown file could not be opened.",
                          L"mwtl Markdown", MB_OK | MB_ICONERROR);
            return false;
        }
        encoding_ = loaded.value->encoding;
        stamp_ = loaded.value->stamp;
        editor_.SetSavePoint();
        document_.MarkOpened(path);
        RefreshPreview();
        UpdatePresentation(L"Opened");
        return true;
    }

    bool SavePath(const std::filesystem::path& path) {
        const auto text = editor_.GetText();
        if (!text) return false;
        const bool same_path = document_.HasPath() && path == document_.GetPath();
        const auto saved = mwtl::WriteTextFileAtomic(
            path, *text, encoding_, same_path ? stamp_ : std::nullopt);
        if (!saved.Succeeded()) {
            const wchar_t* message = saved.status == mwtl::TextFileStatus::changed
                ? L"The file changed outside the editor. It was not overwritten."
                : L"The Markdown file could not be saved.";
            ::MessageBoxW(GetHwnd(), message, L"mwtl Markdown", MB_OK | MB_ICONERROR);
            return false;
        }
        stamp_ = saved.stamp;
        RemoveRecovery();
        editor_.SetSavePoint();
        document_.MarkSavedAs(path);
        UpdatePresentation(L"Saved");
        return true;
    }

    void NewDocument() {
        if (!ConfirmDiscardOrSave()) return;
        editor_.SetText(L"");
        editor_.SetSavePoint();
        document_.ResetUntitled();
        encoding_ = mwtl::TextEncoding::utf8;
        stamp_.reset();
        RefreshPreview();
        UpdatePresentation(L"New document");
        editor_.Focus();
    }

    void OpenInteractive() {
        if (!ConfirmDiscardOrSave()) return;
        const auto selected = mwtl::ShowOpenFileDialog({
            .owner = GetHwnd(), .title = L"Open Markdown",
            .filters = {{L"Markdown files", L"*.md;*.markdown;*.mdown"},
                        {L"Text files", L"*.txt"}, {L"All files", L"*.*"}}});
        if (selected.accepted) static_cast<void>(OpenPath(selected.path));
    }

    bool SaveInteractive() {
        auto path = document_.GetPath();
        if (!document_.HasPath()) return SaveAsInteractive();
        return SavePath(path);
    }

    bool SaveAsInteractive() {
        const auto selected = mwtl::ShowSaveFileDialog({
            .owner = GetHwnd(), .title = L"Save Markdown",
            .filters = {{L"Markdown file", L"*.md"}, {L"All files", L"*.*"}},
            .default_extension = L"md"});
        return selected.accepted && SavePath(selected.path);
    }

    bool ConfirmDiscardOrSave() {
        if (!document_.IsDirty()) return true;
        const int choice = ::MessageBoxW(
            GetHwnd(), L"Save changes to this document?", L"mwtl Markdown",
            MB_YESNOCANCEL | MB_ICONWARNING);
        if (choice == IDCANCEL) return false;
        if (choice == IDYES) return SaveInteractive();
        return choice == IDNO;
    }

    std::optional<mwtl::ScintillaTextRange> FindNext() {
        const auto query = search_.GetText();
        if (query.empty()) {
            search_.Focus();
            status_.SetText(L"Enter text to find");
            return std::nullopt;
        }
        const auto selection = editor_.GetSelection();
        auto found = editor_.Find(query, selection.end);
        if (!found) found = editor_.Find(query);
        if (found) {
            editor_.SetSelection(*found);
            editor_.Focus();
            status_.SetText(L"Match selected");
        } else {
            status_.SetText(L"No match");
        }
        return found;
    }

    void ReplaceNext() {
        const auto found = FindNext();
        if (!found || !editor_.ReplaceTarget(replacement_.GetText())) return;
        const auto utf8 = mwtl::ToUtf8(replacement_.GetText());
        if (utf8) editor_.SetSelection({found->start,
            found->start + static_cast<mwtl::ScintillaPosition>(utf8->size())});
        status_.SetText(L"Replaced one match");
    }

    void WrapSelection(std::wstring_view before, std::wstring_view after,
                       std::wstring_view placeholder) {
        const auto range = editor_.GetSelection();
        const auto text = editor_.GetText();
        if (!range || !text) return;
        const auto utf8 = mwtl::ToUtf8(*text);
        if (!utf8 || static_cast<std::size_t>(range.end) > utf8->size()) return;
        const auto selected_utf8 = std::string_view{*utf8}.substr(
            static_cast<std::size_t>(range.start),
            static_cast<std::size_t>(range.end - range.start));
        const auto selected = mwtl::FromUtf8(selected_utf8).value_or(std::wstring{});
        editor_.SetSelection(range);
        const std::wstring replacement = std::wstring(before) +
            (selected.empty() ? std::wstring(placeholder) : selected) + std::wstring(after);
        if (editor_.ReplaceTarget(replacement)) {
            const auto replacement_utf8 = mwtl::ToUtf8(replacement);
            if (replacement_utf8)
                editor_.SetSelection({range.start,
                    range.start + static_cast<mwtl::ScintillaPosition>(replacement_utf8->size())});
        }
        editor_.Focus();
    }

    void PrefixSelection(std::wstring_view prefix) {
        const auto selection = editor_.GetSelection();
        editor_.SetSelection({selection.start, selection.start});
        if (editor_.ReplaceTarget(prefix)) editor_.SetSelection({selection.start,
            selection.start + static_cast<mwtl::ScintillaPosition>(prefix.size())});
        editor_.Focus();
    }

    void ToggleTheme() {
        theme_ = theme_ == markdown_editor::PreviewTheme::light
            ? markdown_editor::PreviewTheme::dark
            : markdown_editor::PreviewTheme::light;
        theme_button_is_dark_ = theme_ == markdown_editor::PreviewTheme::dark;
        theme_button_.SetText(theme_button_is_dark_ ? L"Light" : L"Dark");
        markdown_syntax_.ApplyTheme(editor_, theme_button_is_dark_
            ? markdown_editor::EditorTheme::dark : markdown_editor::EditorTheme::light);
        RefreshPreview();
        UpdatePresentation(theme_button_is_dark_ ? L"Dark theme" : L"Light theme");
    }

    std::size_t WordCount() const {
        const auto text = editor_.GetText();
        if (!text) return 0;
        std::size_t count = 0;
        bool in_word = false;
        for (const wchar_t ch : *text) {
            const bool whitespace = std::iswspace(ch) != 0;
            if (!whitespace && !in_word) ++count;
            in_word = !whitespace;
        }
        return count;
    }

    void UpdatePresentation(std::wstring_view action) {
        const std::wstring dirty = document_.IsDirty() ? L" *" : L"";
        SetTitle(document_.GetDisplayName() + dirty + L" - mwtl Markdown");
        status_.SetText(std::wstring(action) + L"  |  " + std::to_wstring(WordCount()) +
                        L" words  |  UTF text  |  Local preview");
    }

    void RunSelfTest() noexcept {
        int result = 0;
        try {
            const auto rendered = markdown_editor::RenderMarkdown(
                L"# Test\n\n<script>alert(1)</script>\n\n- [x] safe\n");
            if (rendered.find(L"<h1>Test</h1>") == std::wstring::npos ||
                rendered.find(L"<script>alert") != std::wstring::npos ||
                rendered.find(L"&lt;script&gt;") == std::wstring::npos) result = 1;
            constexpr std::wstring_view syntax_sample =
                L"# Heading\n\n**bold** [link](https://example.com)\n\n> quote\n\n- list\n\n~~~cpp\ncode\n~~~\n";
            if (result == 0 && !editor_.SetText(syntax_sample)) result = 11;
            editor_.Send(4003, 0, -1);  // SCI_COLOURISE from pinned Scintilla 5.6.5.
            const auto utf8 = mwtl::ToUtf8(syntax_sample);
            const auto style_for = [&](std::string_view token) {
                if (!utf8) return -1;
                const auto offset = utf8->find(token);
                return offset == std::string::npos ? -1 :
                    markdown_syntax_.StyleAt(editor_, static_cast<mwtl::ScintillaPosition>(offset));
            };
            if (result == 0 && style_for("# Heading") != markdown_editor::markdown_style::header1)
                result = 100 + style_for("# Heading");
            if (result == 0 && style_for("bold") != markdown_editor::markdown_style::strong)
                result = 13;
            if (result == 0 && style_for("link") != markdown_editor::markdown_style::link)
                result = 14;
            if (result == 0 && style_for("> quote") != markdown_editor::markdown_style::block_quote)
                result = 15;
            if (result == 0 && style_for("- list") != markdown_editor::markdown_style::unordered_list)
                result = 16;
            if (result == 0 && style_for("~~~cpp") != markdown_editor::markdown_style::code_block)
                result = 17;
            self_test_path_ = std::filesystem::temp_directory_path() /
                (L"mwtl-markdown-editor-" + std::to_wstring(::GetCurrentProcessId()) + L".md");
            if (result == 0 && !SavePath(self_test_path_)) result = 2;
            if (result == 0 && !OpenPath(self_test_path_)) result = 3;
            self_test_result_ = result;
            self_test_started_ = true;
            RefreshPreview();
            if (!preview_ready_) return;
        } catch (...) {
            self_test_result_ = 10;
        }
        if (self_test_result_ != 0) ::PostQuitMessage(self_test_result_);
    }

    void FinishSelfTest() noexcept {
        int result = self_test_result_;
        if (result == 0 && (!preview_ready_ || !preview_navigation_succeeded_ ||
                            preview_.GetWebView() == nullptr)) result = 4;
        ::PostQuitMessage(result);
    }

    mwtl::ScintillaRuntime scintilla_runtime_;
    markdown_editor::MarkdownSyntax markdown_syntax_;
    mwtl::ScintillaEditor editor_;
    mwtl::WebView2Host preview_;
    mwtl::Splitter splitter_;
    mwtl::CommandSet commands_;
    mwtl::AcceleratorTable accelerators_;
    mwtl::Menu menu_;
    mwtl::Button new_, open_, save_, heading_, bold_, italic_, code_, theme_button_;
    mwtl::Button find_next_, replace_next_;
    mwtl::TextBox search_, replacement_;
    mwtl::Label status_;
    mwtl::DocumentState document_{L"Untitled.md"};
    mwtl::TextEncoding encoding_ = mwtl::TextEncoding::utf8;
    std::optional<mwtl::FileStamp> stamp_;
    markdown_editor::PreviewTheme theme_ = markdown_editor::PreviewTheme::light;
    std::wstring pending_html_;
    std::filesystem::path preview_data_folder_;
    std::filesystem::path self_test_path_;
    std::filesystem::path recovery_path_;
    mwtl::UiTimer recovery_timer_;
    bool preview_ready_ = false;
    bool preview_navigation_succeeded_ = false;
    bool theme_button_is_dark_ = false;
    bool self_test_started_ = false;
    int self_test_result_ = 0;
};

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
    const HICON icon = ::LoadIconW(instance, MAKEINTRESOURCEW(IDI_MWTL_MARKDOWN));
    return mwtl::RunApplication<MarkdownEditorWindow>(
        instance, show_command,
        {.title = L"mwtl Markdown",
         .initial_bounds = {{}, {1360.0_dip, 840.0_dip}},
         .use_default_bounds = false,
         .icon = icon,
         .small_icon = icon},
        {.com_apartment = mwtl::ComApartment::sta});
}
