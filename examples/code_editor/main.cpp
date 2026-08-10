#include <mwfl/mwfl.h>
#include <mwfl/scintilla.h>

#include <filesystem>
#include <stdexcept>
#include <string>

using mwfl::operator""_dip;

namespace {
constexpr mwfl::ControlId kOpen{900};
constexpr mwfl::ControlId kSave{901};
constexpr mwfl::ControlId kUndo{902};
constexpr mwfl::ControlId kRedo{903};
constexpr mwfl::ControlId kFind{904};
constexpr mwfl::ControlId kReplace{905};
constexpr mwfl::ControlId kSearch{906};
constexpr mwfl::ControlId kReplacement{907};
constexpr mwfl::ControlId kEditor{908};
constexpr UINT kRunSelfTest = WM_APP + 0x190;

class CodeEditorWindow final : public mwfl::WindowBase {
public:
    void BuildUI() override {
        SetTitle(L"mwfl Code Editor");
        if (!runtime_.LoadAdjacent()) {
            throw std::runtime_error(
                "Scintilla.dll is unavailable beside the executable; enable "
                "MWFL_BUILD_SCINTILLA and use mwfl_deploy_scintilla(target)");
        }

        mwfl::ControlHost ui{*this};
        ui.Add(open_, kOpen, L"Open...");
        ui.Add(save_, kSave, L"Save");
        ui.Add(undo_, kUndo, L"Undo");
        ui.Add(redo_, kRedo, L"Redo");
        ui.Add(search_, kSearch, L"world");
        ui.Add(find_, kFind, L"Find");
        ui.Add(replacement_, kReplacement, L"mwfl");
        ui.Add(replace_, kReplace, L"Replace");
        ui.AddNative(editor_, kEditor, mwfl::RectDip{}, runtime_);
        ui.Add(status_, L"Ready");

        mwfl::Must(editor_.ConfigureCodeEditing(), "configure Scintilla code editing");
        mwfl::Must(mwfl::SetAccessibleName(search_.GetHwnd(), L"Search text"),
                   "name code editor search field");
        mwfl::Must(mwfl::SetAccessibleName(replacement_.GetHwnd(), L"Replacement text"),
                   "name code editor replacement field");
        mwfl::Must(mwfl::SetAccessibleName(editor_.GetHwnd(), L"Source code editor"),
                   "name Scintilla editor");
        mwfl::Must(mwfl::SetAccessibleName(status_.GetHwnd(), L"Document status"),
                   "name code editor status");

        SetLayout(mwfl::Column().Gap(6.0_dip).Margin(8.0_dip)
            .Add(mwfl::Row().Gap(6.0_dip)
                .Add(open_, mwfl::Fixed(82.0_dip)).Add(save_, mwfl::Fixed(72.0_dip))
                .Add(undo_, mwfl::Fixed(72.0_dip)).Add(redo_, mwfl::Fixed(72.0_dip)),
                mwfl::Fixed(30.0_dip))
            .Add(mwfl::Row().Gap(6.0_dip)
                .Add(search_, mwfl::Stretch()).Add(find_, mwfl::Fixed(72.0_dip))
                .Add(replacement_, mwfl::Stretch()).Add(replace_, mwfl::Fixed(82.0_dip)),
                mwfl::Fixed(30.0_dip))
            .Add(editor_, mwfl::Stretch()).Add(status_, mwfl::Auto()));

        editor_.SetText(L"// Unicode: 你好, world!\n#include <mwfl/mwfl.h>\n");
        editor_.SetSavePoint();
        document_.ResetUntitled();
        SyncPresentation(L"Ready");
        editor_.Focus();
        if (IsSelfTest() && ::PostMessageW(GetHwnd(), kRunSelfTest, 0, 0) == FALSE)
            throw std::runtime_error("post code editor self-test failed");
    }

    mwfl::EventResult OnCommand(const mwfl::CommandEvent& event) override {
        if (event.IsClicked(open_)) OpenInteractive();
        else if (event.IsClicked(save_)) static_cast<void>(SaveInteractive());
        else if (event.IsClicked(undo_)) editor_.Undo();
        else if (event.IsClicked(redo_)) editor_.Redo();
        else if (event.IsClicked(find_)) FindNext();
        else if (event.IsClicked(replace_)) ReplaceNext();
        else return mwfl::EventResult::Propagate();
        return mwfl::EventResult::Handled();
    }

    mwfl::EventResult OnNotify(const mwfl::NotifyEvent& event) override {
        if (!event.IsFrom(editor_)) return mwfl::EventResult::Propagate();
        const auto notification = editor_.DecodeNotification(event.header);
        if (!notification) return mwfl::EventResult::Propagate();
        if (notification->kind == mwfl::ScintillaNotificationKind::save_point_left) {
            document_.MarkChanged();
            SyncPresentation(L"Modified");
        } else if (notification->kind == mwfl::ScintillaNotificationKind::save_point_reached) {
            document_.MarkSaved();
            SyncPresentation(L"Saved");
        } else if (notification->kind == mwfl::ScintillaNotificationKind::modified &&
                   notification->lines_added != 0) {
            static_cast<void>(editor_.UpdateLineNumberMargin());
        }
        return mwfl::EventResult::Propagate();
    }

    mwfl::EventResult OnMessage(const mwfl::WindowMessage& event) override {
        if (event.id != kRunSelfTest) return mwfl::EventResult::Propagate();
        RunSelfTest();
        return mwfl::EventResult::Handled();
    }

    mwfl::EventResult OnDpiChanged(const mwfl::DpiChangedEvent&) override {
        static_cast<void>(editor_.ConfigureCodeEditing());
        return mwfl::EventResult::Propagate();
    }

    mwfl::EventResult OnClose() override {
        if (IsSelfTest() || !document_.IsDirty()) return mwfl::EventResult::Propagate();
        const int answer = ::MessageBoxW(GetHwnd(), L"Discard unsaved changes?",
                                         L"mwfl Code Editor", MB_ICONWARNING | MB_YESNO);
        return answer == IDYES ? mwfl::EventResult::Propagate()
                               : mwfl::EventResult::Handled();
    }

private:
    static bool IsSelfTest() noexcept {
        return std::wstring_view{::GetCommandLineW()}.find(L"--self-test") !=
               std::wstring_view::npos;
    }

    bool OpenPath(const std::filesystem::path& path) {
        const auto loaded = mwfl::ReadTextFile(path);
        if (!loaded.Succeeded() || !editor_.SetText(loaded.value->text)) return false;
        editor_.SetSavePoint();
        document_.MarkOpened(path);
        encoding_ = loaded.value->encoding;
        SyncPresentation(L"Opened");
        return true;
    }

    bool SavePath(const std::filesystem::path& path) {
        const auto text = editor_.GetText();
        if (!text) return false;
        const auto saved = mwfl::WriteTextFileAtomic(path, *text, encoding_);
        if (!saved.Succeeded()) return false;
        editor_.SetSavePoint();
        document_.MarkSavedAs(path);
        SyncPresentation(L"Saved");
        return true;
    }

    void OpenInteractive() {
        const auto selected = mwfl::ShowOpenFileDialog({
            .owner = GetHwnd(), .title = L"Open source file",
            .filters = {{L"Source files", L"*.cpp;*.h;*.hpp;*.c;*.txt;*.md"},
                        {L"All files", L"*.*"}}});
        if (selected.accepted && !OpenPath(selected.path)) status_.SetText(L"Open failed");
    }

    bool SaveInteractive() {
        auto path = document_.GetPath();
        if (!document_.HasPath()) {
            const auto selected = mwfl::ShowSaveFileDialog({
                .owner = GetHwnd(), .title = L"Save source file",
                .filters = {{L"C++ source", L"*.cpp"}, {L"All files", L"*.*"}},
                .default_extension = L"cpp"});
            if (!selected.accepted) return false;
            path = selected.path;
        }
        if (!SavePath(path)) status_.SetText(L"Save failed");
        return !document_.IsDirty();
    }

    std::optional<mwfl::ScintillaTextRange> FindNext() {
        const auto selection = editor_.GetSelection();
        auto found = editor_.Find(search_.GetText(), selection.end);
        if (!found) found = editor_.Find(search_.GetText());
        if (found) editor_.SetSelection(*found);
        status_.SetText(found ? L"Match selected" : L"No match");
        return found;
    }

    void ReplaceNext() {
        const auto found = FindNext();
        if (!found || !editor_.ReplaceTarget(replacement_.GetText())) return;
        const auto replacement = mwfl::ToUtf8(replacement_.GetText());
        if (replacement) editor_.SetSelection({found->start,
            found->start + static_cast<mwfl::ScintillaPosition>(replacement->size())});
        status_.SetText(L"Replaced");
    }

    void SyncPresentation(std::wstring_view action) {
        const std::wstring dirty = document_.IsDirty() ? L" *" : L"";
        SetTitle(document_.GetDisplayName() + dirty + L" - mwfl Code Editor");
        status_.SetText(std::wstring(action) + (document_.IsDirty() ? L" | modified" : L" | clean"));
    }

    void RunSelfTest() noexcept {
        int result = 0;
        const auto path = std::filesystem::temp_directory_path() /
            (L"mwfl-code-editor-" + std::to_wstring(::GetCurrentProcessId()) + L".cpp");
        try {
            const auto created = mwfl::WriteTextFileAtomic(
                path, L"int main() { // 你好 world\n return 0;\n}\n", mwfl::TextEncoding::utf8);
            if (!created.Succeeded() || !OpenPath(path)) result = 1;
            const auto opened = editor_.GetText();
            if (result == 0 && (!opened || opened->find(L"你好 world") == std::wstring::npos)) result = 2;
            search_.SetText(L"world");
            replacement_.SetText(L"mwfl");
            ReplaceNext();
            if (result == 0 && !editor_.IsModified()) result = 3;
            if (result == 0 && !SavePath(path)) result = 4;
            const auto saved = mwfl::ReadTextFile(path);
            if (result == 0 && (!saved.Succeeded() ||
                saved.value->text.find(L"你好 mwfl") == std::wstring::npos)) result = 5;
            editor_.SetText(L"changed");
            if (result == 0 && !editor_.CanUndo()) result = 6;
            editor_.Undo();
            if (result == 0 && !editor_.CanRedo()) result = 7;
            editor_.Redo();
            editor_.SetZoom(2);
            if (result == 0 && editor_.GetZoom() != 2) result = 8;
        } catch (...) {
            result = 9;
        }
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
        ::PostQuitMessage(result);
    }

    mwfl::ScintillaRuntime runtime_;
    mwfl::ScintillaEditor editor_;
    mwfl::DocumentState document_{L"Untitled.cpp"};
    mwfl::TextEncoding encoding_ = mwfl::TextEncoding::utf8;
    mwfl::Button open_, save_, undo_, redo_, find_, replace_;
    mwfl::TextBox search_, replacement_;
    mwfl::Label status_;
};
}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
    return mwfl::RunApplication<CodeEditorWindow>(
        instance, show_command, {}, {.com_apartment = mwfl::ComApartment::sta});
}
