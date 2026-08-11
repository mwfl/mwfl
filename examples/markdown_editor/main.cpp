#include <mwfl/mwfl.h>
#include <mwfl/scintilla.h>
#include <mwfl/webview2.h>

#include "markdown_renderer.h"
#include "markdown_syntax.h"
#include "resource.h"

#include <algorithm>
#include <chrono>
#include <cwctype>
#include <filesystem>
#include <limits>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

using mwfl::operator""_dip;

namespace {

constexpr mwfl::ControlId kNew{1200};
constexpr mwfl::ControlId kOpen{1201};
constexpr mwfl::ControlId kSave{1202};
constexpr mwfl::ControlId kBold{1203};
constexpr mwfl::ControlId kItalic{1204};
constexpr mwfl::ControlId kCode{1205};
constexpr mwfl::ControlId kHeading{1206};
constexpr mwfl::ControlId kTheme{1207};
constexpr mwfl::ControlId kSaveAs{1208};
constexpr mwfl::ControlId kExit{1209};
constexpr mwfl::ControlId kTabs{1210};
constexpr mwfl::ControlId kEditor{1211};
constexpr mwfl::ControlId kPreview{1212};
constexpr mwfl::ControlId kUndo{1213};
constexpr mwfl::ControlId kRedo{1214};
constexpr mwfl::ControlId kCut{1215};
constexpr mwfl::ControlId kCopy{1216};
constexpr mwfl::ControlId kPaste{1217};
constexpr mwfl::ControlId kSelectAll{1218};
constexpr mwfl::ControlId kFindFocus{1219};
constexpr mwfl::ControlId kReplaceFocus{1220};
constexpr mwfl::ControlId kFindNext{1221};
constexpr mwfl::ControlId kReplaceNext{1222};
constexpr mwfl::ControlId kSearchText{1223};
constexpr mwfl::ControlId kReplacementText{1224};
constexpr UINT kRunSelfTest = WM_APP + 0x220;
constexpr UINT kFinishSelfTest = WM_APP + 0x221;
constexpr mwfl::TimerId kRecoveryTimer{1225};
constexpr mwfl::ControlId kPreviewMode{1226};
constexpr mwfl::ControlId kCloseTab{1227};
constexpr wchar_t kSettingsKey[] = L"Software\\mwfl\\MarkdownEditor";

struct MarkdownDocument {
    mwfl::TabId id{};
    mwfl::DocumentState state{L"Untitled.md"};
    std::wstring text;
    mwfl::ScintillaTextRange selection{0, 0};
    mwfl::TextEncoding encoding = mwfl::TextEncoding::utf8;
    std::optional<mwfl::FileStamp> stamp;
};

class MarkdownEditorWindow final : public mwfl::WindowBase {
   public:
    void BuildUI() override {
        SetTitle(L"Untitled.md - MWFL Markdown");
        BuildCommands();
        if (!scintilla_runtime_.LoadAdjacent()) {
            throw std::runtime_error(
                "Scintilla.dll is unavailable beside the executable; enable "
                "MWFL_BUILD_SCINTILLA and deploy the runtime");
        }

        mwfl::ControlHost ui{*this};
        ui.Add(new_, kNew, L"New");
        ui.Add(open_, kOpen, L"Open...");
        ui.Add(save_, kSave, L"Save");
        ui.Add(heading_, kHeading, L"H1");
        ui.Add(bold_, kBold, L"Bold");
        ui.Add(italic_, kItalic, L"Italic");
        ui.Add(code_, kCode, L"Code");
        ui.Add(preview_button_, kPreviewMode, L"Preview");
        ui.Add(theme_button_, kTheme, L"Dark");
        ui.Add(search_, kSearchText, L"");
        ui.Add(find_next_, kFindNext, L"Find next");
        ui.Add(replacement_, kReplacementText, L"");
        ui.Add(replace_next_, kReplaceNext, L"Replace");
        ui.Add(tabs_, kTabs);
        ui.AddNative(editor_, kEditor, mwfl::RectDip{}, scintilla_runtime_);
        ui.AddNative(preview_, kPreview, mwfl::RectDip{});
        ui.Add(status_, L"Starting preview...");

        mwfl::Must(editor_.ConfigureCodeEditing({.font = L"Segoe UI",
                                                 .font_size_points = 12.0f,
                                                 .tab_width = 2,
                                                 .use_tabs = false,
                                                 .word_wrap = true,
                                                 .show_line_numbers = false}),
                   "configure Markdown source editor");
        if (!markdown_syntax_.Attach(editor_)) {
            throw std::runtime_error(
                "Lexilla.dll or its Markdown lexer is unavailable beside the executable");
        }
        markdown_syntax_.ApplyTheme(editor_, markdown_editor::EditorTheme::light);
        mwfl::Must(mwfl::SetAccessibleName(editor_.GetHwnd(), L"Markdown source"),
                   "name Markdown source editor");
        mwfl::Must(mwfl::SetAccessibleName(preview_.GetHwnd(), L"Markdown preview"),
                   "name Markdown preview");
        mwfl::Must(mwfl::SetAccessibleName(status_.GetHwnd(), L"Document status"),
                   "name Markdown status");
        mwfl::Must(mwfl::SetAccessibleName(search_.GetHwnd(), L"Find text"),
                   "name Markdown search field");
        mwfl::Must(mwfl::SetAccessibleName(replacement_.GetHwnd(), L"Replacement text"),
                   "name Markdown replacement field");
        mwfl::Must(mwfl::SetAccessibleName(tabs_.GetHwnd(), L"Markdown documents"),
                   "name Markdown document tabs");
        mwfl::Must(search_.SetCueBanner(L"Find"), "set Markdown search hint");
        mwfl::Must(replacement_.SetCueBanner(L"Replace with"), "set Markdown replacement hint");
        BuildMenu();
        mwfl::Must(accelerators_.Create(commands_), "create Markdown accelerator table");
        SetAccelerators(accelerators_.GetHandle());
        mwfl::ApplyWindowAppearance(GetHwnd(), {mwfl::ColorMode::light, mwfl::Backdrop::mica});

        SetSearchPanelVisible(false);
        ApplyLayout();

        const std::wstring welcome =
            L"# Shipping a native Windows app\n\n"
            L"MWFL keeps the parts developers value about Win32—real **HWNDs**, "
            L"native accessibility, and direct interop—while making everyday UI "
            L"composition feel like modern C++20.\n\n"
            L"## Release checklist\n\n"
            L"- [x] Responsive layout in device-independent pixels\n"
            L"- [x] Typed commands, menus, and accelerators\n"
            L"- [x] Atomic UTF-8 saves and crash recovery\n"
            L"- [ ] Invite the first public-preview users\n\n"
            L"> Build native Windows software without hiding Windows from you.\n\n"
            L"```cpp\n"
            L"SetLayout(mwfl::Column()\n"
            L"    .Margin(24_dip).Gap(12_dip)\n"
            L"    .Add(editor, mwfl::Stretch()));\n"
            L"```\n\n"
            L"**47 compiled examples** cover controls, documents, docking, Shell "
            L"integration, graphics, WebView2, and Scintilla.\n";
        documents_.push_back({.id = active_tab_, .text = welcome});
        mwfl::Must(editor_.SetText(welcome), "set Markdown welcome document");
        editor_.SetSavePoint();
        mwfl::Must(tabs_model_.Add({active_tab_, L"Untitled.md", false, true}),
                   "add initial Markdown tab");
        mwfl::Must(tabs_.Synchronize(tabs_model_), "show initial Markdown tab");
        SetPreviewMode(false);
        InitializeRecovery();
        StartPreview();
        UpdatePresentation(L"Ready");
        editor_.Focus();
        mwfl::SavedWindowPlacement placement;
        if (!IsSelfTest() && !IsShowcase() &&
            mwfl::LoadWindowPlacementFromRegistry(HKEY_CURRENT_USER, kSettingsKey,
                                                  L"WindowPlacement", placement))
            mwfl::RestoreWindowPlacement(GetHwnd(), placement);

        if (IsSelfTest() && ::PostMessageW(GetHwnd(), kRunSelfTest, 0, 0) == FALSE)
            throw std::runtime_error("post Markdown editor self-test failed");
    }

    mwfl::EventResult OnCommand(const mwfl::CommandEvent& event) override {
        return commands_.Dispatch(event);
    }

    mwfl::EventResult OnNotify(const mwfl::NotifyEvent& event) override {
        if (event.Is(tabs_, TCN_SELCHANGE)) {
            if (const auto selected = tabs_.GetSelectedTabId()) SwitchTo(*selected);
            return mwfl::EventResult::Handled();
        }
        if (!event.IsFrom(editor_)) return mwfl::EventResult::Propagate();
        if (switching_documents_) return mwfl::EventResult::Handled();
        const auto notification = editor_.DecodeNotification(event.header);
        if (!notification) return mwfl::EventResult::Propagate();
        if (notification->kind == mwfl::ScintillaNotificationKind::save_point_left) {
            ActiveDocument().state.MarkChanged();
            static_cast<void>(tabs_model_.SetDirty(active_tab_, true));
            static_cast<void>(tabs_.Synchronize(tabs_model_));
            UpdatePresentation(L"Modified");
        } else if (notification->kind == mwfl::ScintillaNotificationKind::save_point_reached) {
            if (!restored_dirty_active_) {
                ActiveDocument().state.MarkSaved();
                static_cast<void>(tabs_model_.SetDirty(active_tab_, false));
                static_cast<void>(tabs_.Synchronize(tabs_model_));
                UpdatePresentation(L"Saved");
            }
        } else if (notification->kind == mwfl::ScintillaNotificationKind::modified) {
            RefreshPreview();
            UpdatePresentation(L"Editing");
            ScheduleRecovery();
        }
        return mwfl::EventResult::Propagate();
    }

    mwfl::EventResult OnMessage(const mwfl::WindowMessage& event) override {
        if (event.id == kRunSelfTest) {
            RunSelfTest();
            return mwfl::EventResult::Handled();
        }
        if (event.id == kFinishSelfTest) {
            FinishSelfTest();
            return mwfl::EventResult::Handled();
        }
        return mwfl::EventResult::Propagate();
    }

    mwfl::EventResult OnDpiChanged(const mwfl::DpiChangedEvent&) override {
        static_cast<void>(editor_.ConfigureCodeEditing({.font = L"Segoe UI",
                                                        .font_size_points = 12.0f,
                                                        .tab_width = 2,
                                                        .word_wrap = true,
                                                        .show_line_numbers = false}));
        markdown_syntax_.ApplyTheme(editor_, theme_ == markdown_editor::PreviewTheme::dark
                                                 ? markdown_editor::EditorTheme::dark
                                                 : markdown_editor::EditorTheme::light);
        return mwfl::EventResult::Propagate();
    }

    mwfl::EventResult OnTimer(mwfl::TimerId id) override {
        if (id != kRecoveryTimer) return mwfl::EventResult::Propagate();
        recovery_timer_.Stop();
        WriteRecovery();
        return mwfl::EventResult::Handled();
    }

    mwfl::EventResult OnClose() override {
        if (!IsSelfTest() && !ConfirmAllDocuments()) return mwfl::EventResult::Handled();
        recovery_timer_.Stop();
        RemoveRecovery();
        if (!IsSelfTest() && !IsShowcase()) {
            mwfl::SavedWindowPlacement placement;
            if (mwfl::CaptureWindowPlacement(GetHwnd(), placement))
                static_cast<void>(mwfl::SaveWindowPlacementToRegistry(
                    HKEY_CURRENT_USER, kSettingsKey, L"WindowPlacement", placement));
        }
        preview_.Close();
        if (IsSelfTest()) {
            std::error_code ignored;
            if (!self_test_path_.empty()) std::filesystem::remove(self_test_path_, ignored);
            if (!preview_data_folder_.empty())
                std::filesystem::remove_all(preview_data_folder_, ignored);
        }
        return mwfl::EventResult::Propagate();
    }

   private:
    void BuildCommands() {
        commands_
            .Add(mwfl::Command(kNew, L"&New", [this] { NewDocument(); })
                     .SetShortcut({FVIRTKEY | FCONTROL, 'N'}))
            .Add(mwfl::Command(kOpen, L"&Open...", [this] { OpenInteractive(); })
                     .SetShortcut({FVIRTKEY | FCONTROL, 'O'}))
            .Add(mwfl::Command(kCloseTab, L"&Close tab", [this] { CloseActiveTab(); })
                     .SetShortcut({FVIRTKEY | FCONTROL, 'W'}))
            .Add(mwfl::Command(kSave, L"&Save", [this] { static_cast<void>(SaveInteractive()); })
                     .SetShortcut({FVIRTKEY | FCONTROL, 'S'}))
            .Add(mwfl::Command(kSaveAs, L"Save &As...",
                               [this] { static_cast<void>(SaveAsInteractive()); })
                     .SetShortcut({FVIRTKEY | FCONTROL | FSHIFT, 'S'}))
            .Add(mwfl::Command(kExit, L"E&xit", [this] { static_cast<void>(Close()); }))
            .Add(mwfl::Command(kUndo, L"&Undo", [this] { editor_.Undo(); })
                     .SetShortcut({FVIRTKEY | FCONTROL, 'Z'}))
            .Add(mwfl::Command(kRedo, L"&Redo", [this] { editor_.Redo(); })
                     .SetShortcut({FVIRTKEY | FCONTROL, 'Y'}))
            .Add(mwfl::Command(kCut, L"Cu&t", [this] { editor_.Cut(); })
                     .SetShortcut({FVIRTKEY | FCONTROL, 'X'}))
            .Add(mwfl::Command(kCopy, L"&Copy", [this] { editor_.Copy(); })
                     .SetShortcut({FVIRTKEY | FCONTROL, 'C'}))
            .Add(mwfl::Command(kPaste, L"&Paste", [this] { editor_.Paste(); })
                     .SetShortcut({FVIRTKEY | FCONTROL, 'V'}))
            .Add(mwfl::Command(kSelectAll, L"Select &All", [this] { editor_.SelectAll(); })
                     .SetShortcut({FVIRTKEY | FCONTROL, 'A'}))
            .Add(mwfl::Command(kFindFocus, L"&Find", [this] { ShowSearchPanel(false); })
                     .SetShortcut({FVIRTKEY | FCONTROL, 'F'}))
            .Add(mwfl::Command(kReplaceFocus, L"&Replace", [this] { ShowSearchPanel(true); })
                     .SetShortcut({FVIRTKEY | FCONTROL, 'H'}))
            .Add(mwfl::Command(kFindNext, L"Find &next", [this] { static_cast<void>(FindNext()); })
                     .SetShortcut({FVIRTKEY, VK_F3}))
            .Add(mwfl::Command(kReplaceNext, L"Replace next", [this] { ReplaceNext(); }))
            .Add(mwfl::Command(kHeading, L"Heading 1", [this] { PrefixSelection(L"# "); }))
            .Add(mwfl::Command(kBold, L"&Bold",
                               [this] { WrapSelection(L"**", L"**", L"bold text"); })
                     .SetShortcut({FVIRTKEY | FCONTROL, 'B'}))
            .Add(mwfl::Command(kItalic, L"&Italic",
                               [this] { WrapSelection(L"*", L"*", L"italic text"); })
                     .SetShortcut({FVIRTKEY | FCONTROL, 'I'}))
            .Add(mwfl::Command(kCode, L"Inline &code",
                               [this] { WrapSelection(L"`", L"`", L"code"); }))
            .Add(mwfl::Command(kPreviewMode, L"Toggle &preview",
                               [this] { SetPreviewMode(!preview_mode_); })
                     .SetShortcut({FVIRTKEY | FCONTROL | FSHIFT, 'P'}))
            .Add(mwfl::Command(kTheme, L"Toggle preview theme", [this] {
                     ToggleTheme();
                 }).SetShortcut({FVIRTKEY | FCONTROL, 'D'}));
    }

    void BuildMenu() {
        mwfl::Menu bar, file, edit, format, view;
        mwfl::Must(bar.Create(), "create Markdown menu bar");
        mwfl::Must(file.CreatePopup(), "create Markdown File menu");
        for (const auto id : {kNew, kOpen, kSave, kSaveAs, kCloseTab})
            mwfl::Must(file.AppendCommand(*commands_.Find(id)), "append Markdown File command");
        mwfl::Must(file.AppendSeparator(), "append Markdown File separator");
        mwfl::Must(file.AppendCommand(*commands_.Find(kExit)), "append Markdown Exit command");
        mwfl::Must(edit.CreatePopup(), "create Markdown Edit menu");
        for (const auto id :
             {kUndo, kRedo, kCut, kCopy, kPaste, kSelectAll, kFindFocus, kReplaceFocus, kFindNext})
            mwfl::Must(edit.AppendCommand(*commands_.Find(id)), "append Markdown Edit command");
        mwfl::Must(format.CreatePopup(), "create Markdown Format menu");
        for (const auto id : {kHeading, kBold, kItalic, kCode})
            mwfl::Must(format.AppendCommand(*commands_.Find(id)), "append Markdown Format command");
        mwfl::Must(view.CreatePopup(), "create Markdown View menu");
        mwfl::Must(view.AppendCommand(*commands_.Find(kPreviewMode)),
                   "append Markdown preview command");
        mwfl::Must(view.AppendCommand(*commands_.Find(kTheme)), "append Markdown View command");
        mwfl::Must(bar.AppendSubmenu(std::move(file), L"&File"), "append Markdown File menu");
        mwfl::Must(bar.AppendSubmenu(std::move(edit), L"&Edit"), "append Markdown Edit menu");
        mwfl::Must(bar.AppendSubmenu(std::move(format), L"F&ormat"), "append Markdown Format menu");
        mwfl::Must(bar.AppendSubmenu(std::move(view), L"&View"), "append Markdown View menu");
        mwfl::Must(bar.AttachToWindow(GetHwnd()), "attach Markdown menu");
        menu_ = std::move(bar);
    }

    void ApplyLayout() {
        auto column = mwfl::Column().Gap(4.0_dip).Margin(6.0_dip).Add(
            mwfl::Row()
                .Gap(4.0_dip)
                .Add(new_, mwfl::Fixed(44.0_dip))
                .Add(open_, mwfl::Fixed(58.0_dip))
                .Add(save_, mwfl::Fixed(48.0_dip))
                .Add(heading_, mwfl::Fixed(38.0_dip))
                .Add(bold_, mwfl::Fixed(44.0_dip))
                .Add(italic_, mwfl::Fixed(44.0_dip))
                .Add(code_, mwfl::Fixed(44.0_dip))
                .Add(preview_button_, mwfl::Fixed(58.0_dip))
                .Add(theme_button_, mwfl::Fixed(48.0_dip)),
            mwfl::Fixed(24.0_dip));
        if (search_panel_visible_) {
            column.Add(mwfl::Row()
                           .Gap(6.0_dip)
                           .Add(search_, mwfl::Stretch())
                           .Add(find_next_, mwfl::Fixed(86.0_dip))
                           .Add(replacement_, mwfl::Stretch())
                           .Add(replace_next_, mwfl::Fixed(82.0_dip)),
                       mwfl::Fixed(28.0_dip));
        }
        column.Add(tabs_, mwfl::Fixed(28.0_dip))
            .Add(mwfl::Overlay().Add(editor_).Add(preview_), mwfl::Stretch())
            .Add(status_, mwfl::Auto());
        SetLayout(std::move(column));
    }

    void SetSearchPanelVisible(bool visible) {
        search_panel_visible_ = visible;
        search_.SetVisible(visible);
        replacement_.SetVisible(visible);
        find_next_.SetVisible(visible);
        replace_next_.SetVisible(visible);
    }

    void ShowSearchPanel(bool replacement) {
        if (!search_panel_visible_) {
            SetSearchPanelVisible(true);
            ApplyLayout();
        }
        auto& field = replacement ? replacement_ : search_;
        field.Focus();
        field.SelectAll();
    }

    static bool IsSelfTest() noexcept {
        return std::wstring_view{::GetCommandLineW()}.find(L"--self-test") !=
               std::wstring_view::npos;
    }

    static bool IsShowcase() noexcept {
        return std::wstring_view{::GetCommandLineW()}.find(L"--showcase") !=
               std::wstring_view::npos;
    }

    void StartPreview() {
        if (IsSelfTest()) {
            preview_data_folder_ =
                std::filesystem::temp_directory_path() /
                (L"mwfl-markdown-editor-webview-" + std::to_wstring(::GetCurrentProcessId()));
        }
        const bool started = preview_.Initialize(
            {.user_data_folder = preview_data_folder_},
            {.initialized =
                 [this](mwfl::WebView2InitializationResult result) {
                     if (result.state != mwfl::WebView2HostState::ready) {
                         status_.SetText(result.runtime == mwfl::WebView2RuntimeStatus::missing
                                             ? L"WebView2 Runtime is missing; editing and saving "
                                               L"remain available"
                                             : L"Preview initialization failed; editing and saving "
                                               L"remain available");
                         if (IsSelfTest())
                             ::PostQuitMessage(
                                 result.runtime == mwfl::WebView2RuntimeStatus::missing ? 0 : 20);
                         return;
                     }
                     preview_ready_ = true;
                     RefreshPreview();
                 },
             .navigation_completed =
                 [this](bool succeeded, HRESULT) {
                     preview_navigation_succeeded_ = succeeded;
                     if (IsSelfTest() && self_test_started_ &&
                         ::PostMessageW(GetHwnd(), kFinishSelfTest, 0, 0) == FALSE)
                         ::PostQuitMessage(21);
                 },
             .process_failed =
                 [this](mwfl::WebView2ProcessFailureKind) {
                     preview_ready_ = false;
                     status_.SetText(
                         L"Preview process stopped; press the theme button to retry rendering");
                 }});
        if (!started) status_.SetText(L"Preview could not start; editor remains available");
    }

    void InitializeRecovery() {
        if (IsSelfTest()) return;
        wchar_t local_data[32768]{};
        const DWORD length = ::GetEnvironmentVariableW(L"LOCALAPPDATA", local_data,
                                                       static_cast<DWORD>(std::size(local_data)));
        if (length == 0 || length >= std::size(local_data)) return;
        recovery_directory_ =
            std::filesystem::path{local_data} / L"MWFL" / L"MarkdownEditor" / L"Recovery";
        session_path_ = recovery_directory_ / L"session.mwfl";
        std::error_code error;
        if (!std::filesystem::exists(session_path_, error) || error) return;
        const auto recovered = mwfl::LoadDocumentSession(session_path_);
        if (!recovered || recovered.session->workspaces.size() != 1 ||
            recovered.session->workspaces[0].documents.empty())
            return;
        const int answer = ::MessageBoxW(
            GetHwnd(), L"Documents from an interrupted session were found. Restore all tabs?",
            L"MWFL Markdown", MB_YESNO | MB_ICONINFORMATION);
        if (answer == IDYES && RestoreRecoverySession(recovered.session->workspaces[0]))
            status_.SetText(L"Recovered interrupted document session");
        else
            RemoveRecovery();
    }

    bool RestoreRecoverySession(const mwfl::WorkspaceSession& workspace) {
        std::vector<MarkdownDocument> restored;
        restored.reserve(workspace.documents.size());
        std::uint64_t next_id = 1;
        for (const auto& item : workspace.documents) {
            const auto id = item.metadata.id.value;
            if (id == 0 ||
                item.metadata.view.selection_start >
                    static_cast<std::uint64_t>((std::numeric_limits<std::intptr_t>::max)()) ||
                item.metadata.view.selection_end >
                    static_cast<std::uint64_t>((std::numeric_limits<std::intptr_t>::max)()))
                return false;
            const auto recovery =
                recovery_directory_ / (L"document-" + std::to_wstring(id) + L".md");
            const auto text = mwfl::ReadTextFile(recovery);
            if (!text.Succeeded()) return false;
            MarkdownDocument document;
            document.id = {id};
            document.text = text.value->text;
            document.selection = {
                static_cast<mwfl::ScintillaPosition>(item.metadata.view.selection_start),
                static_cast<mwfl::ScintillaPosition>(item.metadata.view.selection_end)};
            if (!item.metadata.path.empty()) {
                document.state.MarkOpened(item.metadata.path);
                const auto original = mwfl::ReadTextFile(item.metadata.path);
                if (original.Succeeded()) document.encoding = original.value->encoding;
            }
            document.stamp = item.stamp;
            if (item.metadata.dirty) document.state.MarkChanged();
            restored.push_back(std::move(document));
            next_id = (std::max)(next_id, id + 1);
        }
        if (restored.empty()) return false;

        std::vector<mwfl::TabId> old_tabs;
        for (const auto& tab : tabs_model_.GetTabs()) old_tabs.push_back(tab.id);
        for (const auto id : old_tabs) static_cast<void>(tabs_model_.Remove(id));
        documents_ = std::move(restored);
        for (const auto& document : documents_) {
            if (!tabs_model_.Add(
                    {document.id, document.state.GetDisplayName(), document.state.IsDirty(), true}))
                return false;
        }
        const mwfl::TabId requested{workspace.active.value_or(mwfl::DocumentId{}).value};
        const mwfl::TabId selected = FindDocument(requested) ? requested : documents_.front().id;
        static_cast<void>(tabs_model_.Select(selected));
        static_cast<void>(tabs_.Synchronize(tabs_model_));
        active_tab_ = {};
        next_tab_id_ = next_id;
        SwitchTo(selected);
        return true;
    }

    void ScheduleRecovery() {
        if (IsSelfTest() || session_path_.empty() || recovery_timer_.IsRunning()) return;
        static_cast<void>(
            recovery_timer_.Start(*this, kRecoveryTimer, std::chrono::milliseconds{1500}));
    }

    void WriteRecovery() {
        if (session_path_.empty()) return;
        StoreActiveView();
        const bool any_dirty = std::ranges::any_of(
            documents_, [](const auto& document) { return document.state.IsDirty(); });
        if (!any_dirty) {
            RemoveRecovery();
            return;
        }
        std::error_code error;
        std::filesystem::create_directories(recovery_directory_, error);
        if (error) return;
        mwfl::DocumentSession session;
        mwfl::WorkspaceSession workspace;
        workspace.workspace = {1};
        workspace.active = mwfl::DocumentId{active_tab_.value};
        for (const auto& document : documents_) {
            const auto recovery =
                recovery_directory_ / (L"document-" + std::to_wstring(document.id.value) + L".md");
            if (!mwfl::WriteTextFileAtomic(recovery, document.text, mwfl::TextEncoding::utf8)
                     .Succeeded())
                return;
            mwfl::SessionDocument item;
            item.metadata.id = {document.id.value};
            item.metadata.title = document.state.GetDisplayName();
            item.metadata.path = document.state.GetPath();
            item.metadata.dirty = document.state.IsDirty();
            item.metadata.view.selection_start = static_cast<std::uint64_t>(
                (std::max)(mwfl::ScintillaPosition{}, document.selection.start));
            item.metadata.view.selection_end = static_cast<std::uint64_t>(
                (std::max)(mwfl::ScintillaPosition{}, document.selection.end));
            item.stamp = document.stamp;
            workspace.documents.push_back(std::move(item));
        }
        session.workspaces.push_back(std::move(workspace));
        static_cast<void>(mwfl::SaveDocumentSessionAtomic(session_path_, session));
    }

    void RemoveRecovery() noexcept {
        if (recovery_directory_.empty()) return;
        std::error_code ignored;
        std::filesystem::remove_all(recovery_directory_, ignored);
    }

    void RefreshPreview() {
        const auto text = editor_.GetText();
        if (!text) return;
        markdown_editor::RenderOptions options{.theme = theme_};
        if (ActiveDocument().state.HasPath())
            options.document_directory = ActiveDocument().state.GetPath().parent_path();
        pending_html_ = markdown_editor::RenderMarkdown(*text, options);
        if (preview_ready_ && !preview_.NavigateToString(pending_html_))
            status_.SetText(L"Preview update failed; document text is safe");
    }

    MarkdownDocument* FindDocument(mwfl::TabId id) noexcept {
        const auto found =
            std::find_if(documents_.begin(), documents_.end(),
                         [id](const MarkdownDocument& document) { return document.id == id; });
        return found == documents_.end() ? nullptr : &*found;
    }

    MarkdownDocument& ActiveDocument() {
        auto* document = FindDocument(active_tab_);
        if (!document) throw std::runtime_error("active Markdown tab has no document");
        return *document;
    }

    const MarkdownDocument& ActiveDocument() const {
        const auto found = std::find_if(
            documents_.begin(), documents_.end(),
            [this](const MarkdownDocument& document) { return document.id == active_tab_; });
        if (found == documents_.end())
            throw std::runtime_error("active Markdown tab has no document");
        return *found;
    }

    void StoreActiveView() {
        if (documents_.empty()) return;
        auto& document = ActiveDocument();
        if (const auto text = editor_.GetText()) document.text = *text;
        document.selection = editor_.GetSelection();
    }

    void SwitchTo(mwfl::TabId id) {
        if (id == active_tab_ || !FindDocument(id)) return;
        if (FindDocument(active_tab_)) StoreActiveView();
        active_tab_ = id;
        auto& document = ActiveDocument();
        switching_documents_ = true;
        const bool loaded = editor_.SetText(document.text);
        editor_.SetSavePoint();
        static_cast<void>(editor_.SetSelection(document.selection));
        switching_documents_ = false;
        if (!loaded) throw std::runtime_error("switch Markdown document text");
        restored_dirty_active_ = document.state.IsDirty();
        static_cast<void>(tabs_model_.Select(id));
        static_cast<void>(tabs_.SetSelection(id));
        RefreshPreview();
        UpdatePresentation(L"Switched document");
        if (!preview_mode_) editor_.Focus();
    }

    bool OpenPath(const std::filesystem::path& path) {
        const auto loaded = mwfl::ReadTextFile(path);
        if (!loaded.Succeeded()) {
            ::MessageBoxW(GetHwnd(), L"The Markdown file could not be opened.", L"MWFL Markdown",
                          MB_OK | MB_ICONERROR);
            return false;
        }
        StoreActiveView();
        const mwfl::TabId id{next_tab_id_++};
        MarkdownDocument opened;
        opened.id = id;
        opened.state.MarkOpened(path);
        opened.text = loaded.value->text;
        opened.encoding = loaded.value->encoding;
        opened.stamp = loaded.value->stamp;
        documents_.push_back(std::move(opened));
        mwfl::Must(tabs_model_.Add({id, path.filename().wstring(), false, true}),
                   "add opened Markdown tab");
        static_cast<void>(tabs_model_.Select(id));
        static_cast<void>(tabs_.Synchronize(tabs_model_));
        SwitchTo(id);
        ScheduleRecovery();
        return true;
    }

    bool SavePath(const std::filesystem::path& path) {
        const auto text = editor_.GetText();
        if (!text) return false;
        auto& document = ActiveDocument();
        const bool same_path = document.state.HasPath() && path == document.state.GetPath();
        const auto saved = mwfl::WriteTextFileAtomic(path, *text, document.encoding,
                                                     same_path ? document.stamp : std::nullopt);
        if (!saved.Succeeded()) {
            const wchar_t* message =
                saved.status == mwfl::TextFileStatus::changed
                    ? L"The file changed outside the editor. It was not overwritten."
                    : L"The Markdown file could not be saved.";
            ::MessageBoxW(GetHwnd(), message, L"MWFL Markdown", MB_OK | MB_ICONERROR);
            return false;
        }
        document.stamp = saved.stamp;
        document.text = *text;
        editor_.SetSavePoint();
        restored_dirty_active_ = false;
        document.state.MarkSavedAs(path);
        static_cast<void>(tabs_model_.SetTitle(active_tab_, path.filename().wstring()));
        static_cast<void>(tabs_model_.SetDirty(active_tab_, false));
        static_cast<void>(tabs_.Synchronize(tabs_model_));
        if (std::ranges::any_of(documents_, [](const MarkdownDocument& candidate) {
                return candidate.state.IsDirty();
            }))
            WriteRecovery();
        else
            RemoveRecovery();
        UpdatePresentation(L"Saved");
        return true;
    }

    void NewDocument() {
        if (FindDocument(active_tab_)) StoreActiveView();
        const mwfl::TabId id{next_tab_id_++};
        documents_.push_back({.id = id});
        mwfl::Must(tabs_model_.Add({id, L"Untitled.md", false, true}), "add new Markdown tab");
        static_cast<void>(tabs_model_.Select(id));
        static_cast<void>(tabs_.Synchronize(tabs_model_));
        SwitchTo(id);
        ScheduleRecovery();
    }

    void CloseActiveTab() {
        if (!ConfirmDiscardOrSave()) return;
        const mwfl::TabId closing = active_tab_;
        StoreActiveView();
        static_cast<void>(tabs_model_.Remove(closing));
        std::erase_if(documents_, [closing](const MarkdownDocument& document) {
            return document.id == closing;
        });
        if (documents_.empty()) {
            active_tab_ = {};
            NewDocument();
            return;
        }
        const auto selected = tabs_model_.GetSelectedId().value_or(documents_.front().id);
        static_cast<void>(tabs_.Synchronize(tabs_model_));
        SwitchTo(selected);
        ScheduleRecovery();
    }

    void OpenInteractive() {
        const auto selected =
            mwfl::ShowOpenFileDialog({.owner = GetHwnd(),
                                      .title = L"Open Markdown",
                                      .filters = {{L"Markdown files", L"*.md;*.markdown;*.mdown"},
                                                  {L"Text files", L"*.txt"},
                                                  {L"All files", L"*.*"}}});
        if (selected.accepted) static_cast<void>(OpenPath(selected.path));
    }

    bool SaveInteractive() {
        auto path = ActiveDocument().state.GetPath();
        if (!ActiveDocument().state.HasPath()) return SaveAsInteractive();
        return SavePath(path);
    }

    bool SaveAsInteractive() {
        const auto selected = mwfl::ShowSaveFileDialog(
            {.owner = GetHwnd(),
             .title = L"Save Markdown",
             .filters = {{L"Markdown file", L"*.md"}, {L"All files", L"*.*"}},
             .default_extension = L"md"});
        return selected.accepted && SavePath(selected.path);
    }

    bool ConfirmDiscardOrSave() {
        if (!ActiveDocument().state.IsDirty()) return true;
        const int choice = ::MessageBoxW(GetHwnd(), L"Save changes to this document?",
                                         L"MWFL Markdown", MB_YESNOCANCEL | MB_ICONWARNING);
        if (choice == IDCANCEL) return false;
        if (choice == IDYES) return SaveInteractive();
        return choice == IDNO;
    }

    bool ConfirmAllDocuments() {
        const mwfl::TabId original = active_tab_;
        std::vector<mwfl::TabId> ids;
        ids.reserve(documents_.size());
        for (const auto& document : documents_) ids.push_back(document.id);
        for (const auto id : ids) {
            SwitchTo(id);
            if (!ConfirmDiscardOrSave()) return false;
        }
        SwitchTo(original);
        return true;
    }

    std::optional<mwfl::ScintillaTextRange> FindNext() {
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
        const auto utf8 = mwfl::ToUtf8(replacement_.GetText());
        if (utf8)
            editor_.SetSelection(
                {found->start, found->start + static_cast<mwfl::ScintillaPosition>(utf8->size())});
        status_.SetText(L"Replaced one match");
    }

    void WrapSelection(std::wstring_view before, std::wstring_view after,
                       std::wstring_view placeholder) {
        const auto range = editor_.GetSelection();
        const auto text = editor_.GetText();
        if (!range || !text) return;
        const auto utf8 = mwfl::ToUtf8(*text);
        if (!utf8 || static_cast<std::size_t>(range.end) > utf8->size()) return;
        const auto selected_utf8 =
            std::string_view{*utf8}.substr(static_cast<std::size_t>(range.start),
                                           static_cast<std::size_t>(range.end - range.start));
        const auto selected = mwfl::FromUtf8(selected_utf8).value_or(std::wstring{});
        editor_.SetSelection(range);
        const std::wstring replacement = std::wstring(before) +
                                         (selected.empty() ? std::wstring(placeholder) : selected) +
                                         std::wstring(after);
        if (editor_.ReplaceTarget(replacement)) {
            const auto replacement_utf8 = mwfl::ToUtf8(replacement);
            if (replacement_utf8)
                editor_.SetSelection(
                    {range.start,
                     range.start + static_cast<mwfl::ScintillaPosition>(replacement_utf8->size())});
        }
        editor_.Focus();
    }

    void PrefixSelection(std::wstring_view prefix) {
        const auto selection = editor_.GetSelection();
        editor_.SetSelection({selection.start, selection.start});
        if (editor_.ReplaceTarget(prefix))
            editor_.SetSelection(
                {selection.start,
                 selection.start + static_cast<mwfl::ScintillaPosition>(prefix.size())});
        editor_.Focus();
    }

    void SetPreviewMode(bool preview) {
        preview_mode_ = preview;
        editor_.SetVisible(!preview);
        preview_.SetVisible(preview);
        preview_button_.SetText(preview ? L"Edit" : L"Preview");
        if (auto* command = commands_.Find(kPreviewMode)) {
            command->SetChecked(preview);
            static_cast<void>(menu_.UpdateCommand(*command));
        }
        if (preview) {
            RefreshPreview();
            preview_.FocusContent();
            UpdatePresentation(L"Preview");
        } else {
            editor_.Focus();
            UpdatePresentation(L"Editing");
        }
    }

    void ToggleTheme() {
        theme_ = theme_ == markdown_editor::PreviewTheme::light
                     ? markdown_editor::PreviewTheme::dark
                     : markdown_editor::PreviewTheme::light;
        theme_button_is_dark_ = theme_ == markdown_editor::PreviewTheme::dark;
        theme_button_.SetText(theme_button_is_dark_ ? L"Light" : L"Dark");
        markdown_syntax_.ApplyTheme(editor_, theme_button_is_dark_
                                                 ? markdown_editor::EditorTheme::dark
                                                 : markdown_editor::EditorTheme::light);
        if (!preview_ready_ && preview_.GetState() != mwfl::WebView2HostState::initializing) {
            status_.SetText(L"Restarting preview...");
            static_cast<void>(preview_.Restart());
        } else {
            RefreshPreview();
        }
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
        const std::wstring dirty = ActiveDocument().state.IsDirty() ? L" *" : L"";
        SetTitle(ActiveDocument().state.GetDisplayName() + dirty + L" - MWFL Markdown");
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
                rendered.find(L"&lt;script&gt;") == std::wstring::npos)
                result = 1;
            constexpr std::wstring_view syntax_sample =
                L"# Heading\n\n**bold** [link](https://example.com)\n\n> quote\n\n- "
                L"list\n\n~~~cpp\ncode\n~~~\n";
            if (result == 0 && !editor_.SetText(syntax_sample)) result = 11;
            editor_.Send(4003, 0, -1);  // SCI_COLOURISE from pinned Scintilla 5.6.5.
            const auto utf8 = mwfl::ToUtf8(syntax_sample);
            const auto style_for = [&](std::string_view token) {
                if (!utf8) return -1;
                const auto offset = utf8->find(token);
                return offset == std::string::npos
                           ? -1
                           : markdown_syntax_.StyleAt(editor_,
                                                      static_cast<mwfl::ScintillaPosition>(offset));
            };
            if (result == 0 && style_for("# Heading") != markdown_editor::markdown_style::header1)
                result = 100 + style_for("# Heading");
            if (result == 0 && style_for("bold") != markdown_editor::markdown_style::strong)
                result = 13;
            if (result == 0 && style_for("link") != markdown_editor::markdown_style::link)
                result = 14;
            if (result == 0 && style_for("> quote") != markdown_editor::markdown_style::block_quote)
                result = 15;
            if (result == 0 &&
                style_for("- list") != markdown_editor::markdown_style::unordered_list)
                result = 16;
            if (result == 0 && style_for("~~~cpp") != markdown_editor::markdown_style::code_block)
                result = 17;
            const mwfl::TabId source_tab = active_tab_;
            if (result == 0) {
                NewDocument();
                if (tabs_model_.GetCount() != 2 || editor_.GetText() != L"") result = 18;
            }
            const mwfl::TabId empty_tab = active_tab_;
            if (result == 0) {
                SwitchTo(source_tab);
                if (editor_.GetText() != syntax_sample) result = 19;
            }
            if (result == 0) {
                SwitchTo(empty_tab);
                if (editor_.GetText() != L"") result = 20;
                SwitchTo(source_tab);
            }
            if (result == 0) {
                SetPreviewMode(true);
                if (::IsWindowVisible(preview_.GetHwnd()) == FALSE ||
                    ::IsWindowVisible(editor_.GetHwnd()) != FALSE)
                    result = 21;
                SetPreviewMode(false);
                if (::IsWindowVisible(editor_.GetHwnd()) == FALSE ||
                    ::IsWindowVisible(preview_.GetHwnd()) != FALSE)
                    result = 22;
            }
            if (result == 0) {
                ActiveDocument().state.MarkChanged();
                static_cast<void>(tabs_model_.SetDirty(active_tab_, true));
                recovery_directory_ =
                    std::filesystem::temp_directory_path() /
                    (L"mwfl-markdown-recovery-" + std::to_wstring(::GetCurrentProcessId()));
                session_path_ = recovery_directory_ / L"session.mwfl";
                WriteRecovery();
                const auto recovered = mwfl::LoadDocumentSession(session_path_);
                if (!recovered || recovered.session->workspaces.size() != 1 ||
                    recovered.session->workspaces[0].documents.size() != 2 ||
                    !RestoreRecoverySession(recovered.session->workspaces[0]) ||
                    editor_.GetText() != syntax_sample)
                    result = 23;
                RemoveRecovery();
            }
            self_test_path_ =
                std::filesystem::temp_directory_path() /
                (L"mwfl-markdown-editor-" + std::to_wstring(::GetCurrentProcessId()) + L".md");
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
        if (result == 0 &&
            (!preview_ready_ || !preview_navigation_succeeded_ || preview_.GetWebView() == nullptr))
            result = 4;
        ::PostQuitMessage(result);
    }

    mwfl::ScintillaRuntime scintilla_runtime_;
    markdown_editor::MarkdownSyntax markdown_syntax_;
    mwfl::ScintillaEditor editor_;
    mwfl::WebView2Host preview_;
    mwfl::TabControl tabs_;
    mwfl::TabWorkspaceModel tabs_model_;
    mwfl::CommandSet commands_;
    mwfl::AcceleratorTable accelerators_;
    mwfl::Menu menu_;
    mwfl::Button new_, open_, save_, heading_, bold_, italic_, code_, preview_button_,
        theme_button_;
    mwfl::Button find_next_, replace_next_;
    mwfl::TextBox search_, replacement_;
    mwfl::Label status_;
    mwfl::TabId active_tab_{1};
    std::uint64_t next_tab_id_ = 2;
    std::vector<MarkdownDocument> documents_;
    markdown_editor::PreviewTheme theme_ = markdown_editor::PreviewTheme::light;
    std::wstring pending_html_;
    std::filesystem::path preview_data_folder_;
    std::filesystem::path self_test_path_;
    std::filesystem::path recovery_directory_;
    std::filesystem::path session_path_;
    mwfl::UiTimer recovery_timer_;
    bool preview_ready_ = false;
    bool preview_navigation_succeeded_ = false;
    bool theme_button_is_dark_ = false;
    bool preview_mode_ = false;
    bool switching_documents_ = false;
    bool restored_dirty_active_ = false;
    bool search_panel_visible_ = false;
    bool self_test_started_ = false;
    int self_test_result_ = 0;
};

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
    const HICON icon = ::LoadIconW(instance, MAKEINTRESOURCEW(IDI_MWFL_MARKDOWN));
    return mwfl::RunApplication<MarkdownEditorWindow>(
        instance, show_command,
        {.title = L"MWFL Markdown",
         .initial_bounds = {{24.0_dip, 24.0_dip}, {1437.0_dip, 868.5_dip}},
         .use_default_bounds = false,
         .icon = icon,
         .small_icon = icon},
        {.com_apartment = mwfl::ComApartment::sta});
}
