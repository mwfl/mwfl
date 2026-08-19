#include "application.h"
#include <mwfl/mwfl.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <unordered_map>

using mwfl::operator""_dip;

namespace {

constexpr mwfl::ControlId kNew{800};
constexpr mwfl::ControlId kSave{801};
constexpr mwfl::ControlId kClose{802};
constexpr mwfl::ControlId kMove{803};
constexpr mwfl::ControlId kLeft{804};
constexpr mwfl::ControlId kRight{805};
constexpr mwfl::ControlId kUndo{806};
constexpr mwfl::ControlId kRedo{807};
constexpr mwfl::ControlId kExit{808};
constexpr mwfl::ControlId kOpen{809};
constexpr mwfl::ControlId kReopen{810};
constexpr mwfl::ControlId kTabs{820};
constexpr UINT kRunSelfTest = WM_APP + 0x160;

bool SamePath(const std::filesystem::path& left,
              const std::filesystem::path& right) {
    auto a = left.lexically_normal().native();
    auto b = right.lexically_normal().native();
    return ::CompareStringOrdinal(a.c_str(), static_cast<int>(a.size()),
        b.c_str(), static_cast<int>(b.size()), TRUE) == CSTR_EQUAL;
}

class WorkspaceWindow;

struct DocumentContent {
    std::wstring text;
    std::optional<mwfl::FileStamp> stamp;
};

struct Coordinator {
    explicit Coordinator(bool test, std::optional<std::filesystem::path> result)
        : self_test(test), result_path(std::move(result)) {
        if (self_test) {
            session_path = std::filesystem::temp_directory_path() /
                (L"mwfl-document-workspace-app-session-" +
                 std::to_wstring(::GetCurrentProcessId()) + L".state");
            std::error_code ignored;
            std::filesystem::remove(session_path, ignored);
        } else {
            std::array<wchar_t, 32768> local{};
            const DWORD length = ::GetEnvironmentVariableW(
                L"LOCALAPPDATA", local.data(), static_cast<DWORD>(local.size()));
            if (length > 0 && length < local.size())
                session_path = std::filesystem::path{local.data()} /
                    L"mwfl" / L"document-workspace.state";
        }
        if (!session_path.empty()) {
            const auto loaded = mwfl::LoadDocumentSession(session_path);
            if (loaded && loaded.session->workspaces.size() == models.size())
                pending_session = std::move(*loaded.session);
        }
    }

    std::array<mwfl::DocumentWorkspaceModel, 2> models{
        mwfl::DocumentWorkspaceModel{{1}, 8},
        mwfl::DocumentWorkspaceModel{{2}, 8}};
    std::unordered_map<std::uint64_t, DocumentContent> contents;
    std::array<WorkspaceWindow*, 2> windows{};
    std::unique_ptr<WorkspaceWindow> secondary;
    std::uint64_t next_id = 1;
    bool self_test = false;
    std::optional<std::filesystem::path> result_path;
    std::filesystem::path session_path;
    std::optional<mwfl::DocumentSession> pending_session;

    void EnsureSecondary();
    bool MoveActive(std::size_t source);
    bool ConfirmShutdown(HWND owner);
    void InitializeWorkspaces();
    bool SaveSession();
    void Report(std::string_view value) const {
        if (result_path) {
            std::ofstream output(*result_path, std::ios::binary | std::ios::trunc);
            output << value;
        }
    }
};

class WorkspaceWindow final : public mwfl::WindowBase {
public:
    WorkspaceWindow(Coordinator& coordinator, std::size_t index, bool primary)
        : coordinator_(coordinator), index_(index), primary_(primary) {}

    void BuildUI() override {
        coordinator_.windows[index_] = this;
        BuildCommands();
        mwfl::ControlHost ui{*this};
        ui.Add(toolbar_);
        ui.Add(tabs_, mwfl::TabControlOptions{});
        ui.Add(status_);
        for (const auto& command : commands_.GetCommands()) {
            if (command.GetId() != kExit)
                mwfl::Must(toolbar_.AddCommand(command), "add workspace toolbar command");
        }
        toolbar_.AutoSize();
        mwfl::Must(adapter_.Attach(tabs_) == mwfl::DocumentTabStatus::success,
                   "attach document tabs");
        BuildMenu();
        mwfl::Must(accelerators_.Create(commands_), "create workspace accelerators");
        SetAccelerators(accelerators_.GetHandle());
        mwfl::Must(mwfl::SetAccessibleName(tabs_.GetHwnd(), L"Open documents"),
                   "name document tabs");
        mwfl::Must(mwfl::SetAccessibleName(status_.GetHwnd(), L"Workspace status"),
                   "name workspace status");
        SetLayout(mwfl::Column()
            .Add(toolbar_, mwfl::Auto())
            .Add(tabs_, mwfl::Stretch())
            .Add(status_, mwfl::Fixed(26.0_dip)));
        if (index_ == 0) {
            coordinator_.EnsureSecondary();
            coordinator_.InitializeWorkspaces();
            if (coordinator_.self_test &&
                !::PostMessageW(GetHwnd(), kRunSelfTest, 0, 0))
                throw std::runtime_error("post workspace self-test failed");
        }
        Sync(L"Ready");
    }

    mwfl::EventResult OnCommand(const mwfl::CommandEvent& event) override {
        if (event.notification == EN_CHANGE && event.control) {
            for (const auto& binding : adapter_.GetPages()) {
                if (binding.page != event.control) continue;
                coordinator_.contents[binding.document.value].text = GetText(binding.page);
                model().SetDirty(binding.document, true);
                model().SetUndoState(binding.document,
                    ::SendMessageW(binding.page, EM_CANUNDO, 0, 0) != 0, false);
                Sync(L"Modified");
                return mwfl::EventResult::Handled();
            }
        }
        return commands_.Dispatch(event);
    }

    mwfl::EventResult OnNotify(const mwfl::NotifyEvent& event) override {
        if (event.Is(tabs_, TCN_SELCHANGE)) {
            adapter_.ActivateNativeSelection(model());
            Sync(L"Active document changed");
            return mwfl::EventResult::Handled();
        }
        return mwfl::EventResult::Propagate();
    }

    mwfl::EventResult OnResize(const mwfl::ResizeEvent&) override {
        adapter_.ArrangePages();
        return mwfl::EventResult::Propagate();
    }

    mwfl::EventResult OnMessage(const mwfl::WindowMessage& event) override {
        if (event.id != kRunSelfTest) return mwfl::EventResult::Propagate();
        try {
            RunSelfTest();
            coordinator_.Report("ok");
            if (coordinator_.secondary && coordinator_.secondary->IsWindow()) {
                coordinator_.secondary->adapter().Detach();
                ::DestroyWindow(coordinator_.secondary->GetHwnd());
                coordinator_.windows[1] = nullptr;
            }
            coordinator_.secondary.reset();
            ::PostQuitMessage(0);
        } catch (const std::exception& error) {
            coordinator_.Report(error.what());
            ::PostQuitMessage(1);
        }
        return mwfl::EventResult::Handled();
    }

    mwfl::EventResult OnClose() override {
        if (primary_) {
            if (!coordinator_.ConfirmShutdown(GetHwnd()))
                return mwfl::EventResult::Handled();
        } else if (!ConfirmCloseAll()) {
            return mwfl::EventResult::Handled();
        }
        if (primary_ && coordinator_.secondary && coordinator_.secondary->IsWindow())
            ::DestroyWindow(coordinator_.secondary->GetHwnd());
        coordinator_.windows[index_] = nullptr;
        return mwfl::EventResult::Propagate();
    }

    mwfl::DocumentWorkspaceModel& model() { return coordinator_.models[index_]; }
    mwfl::DocumentTabWorkspaceAdapter& adapter() { return adapter_; }

    void Sync(std::wstring_view message) {
        const auto projection = mwfl::BuildActiveDocumentCommandProjection(model());
        if (mwfl::ApplyActiveDocumentCommandProjection(
                commands_, {kSave, kClose, kUndo, kRedo}, projection) !=
            mwfl::DocumentCommandProjectionStatus::success)
            throw std::runtime_error("project active document commands failed");
        for (const auto id : {kSave, kClose, kUndo, kRedo})
            if (const auto* command = commands_.Find(id)) toolbar_.UpdateCommand(*command);
        if (!adapter_.Synchronize(model()))
            throw std::runtime_error("synchronize document tabs failed");
        if (file_menu_) {
            for (const auto id : {kSave, kClose}) {
                const auto* command = commands_.Find(id);
                ::EnableMenuItem(file_menu_, id.value, MF_BYCOMMAND |
                    (command && command->IsEnabled() ? MF_ENABLED : MF_GRAYED));
            }
        }
        status_.SetText(std::wstring{message} + L" — " + projection.status_text);
        SetTitle((index_ == 0 ? L"Workspace A" : L"Workspace B") +
                 std::wstring{L" — mwfl Documents"});
        ::DrawMenuBar(GetHwnd());
    }

private:
    friend struct Coordinator;
    void BuildCommands() {
        commands_
            .Add(mwfl::Command(kNew, L"New", [this] { NewDocument(); })
                     .SetShortcut({FVIRTKEY | FCONTROL, 'N'}))
            .Add(mwfl::Command(kOpen, L"Open…", [this] { OpenInteractive(); })
                     .SetShortcut({FVIRTKEY | FCONTROL, 'O'}))
            .Add(mwfl::Command(kSave, L"Save", [this] { SaveActive(); })
                     .SetShortcut({FVIRTKEY | FCONTROL, 'S'}))
            .Add(mwfl::Command(kClose, L"Close", [this] { CloseActive(); })
                     .SetShortcut({FVIRTKEY | FCONTROL, 'W'}))
            .Add(mwfl::Command(kMove, L"Move to other window",
                               [this] { coordinator_.MoveActive(index_); }))
            .Add(mwfl::Command(kLeft, L"Move tab left", [this] { Reorder(-1); }))
            .Add(mwfl::Command(kRight, L"Move tab right", [this] { Reorder(1); }))
            .Add(mwfl::Command(kReopen, L"Reopen closed", [this] { ReopenClosed(); })
                     .SetShortcut({FVIRTKEY | FCONTROL | FSHIFT, 'T'}))
            .Add(mwfl::Command(kUndo, L"Undo", [this] {
                if (const HWND page = ActivePage()) ::SendMessageW(page, WM_UNDO, 0, 0);
            }).SetShortcut({FVIRTKEY | FCONTROL, 'Z'}))
            .Add(mwfl::Command(kRedo, L"Redo", [] {}).SetEnabled(false))
            .Add(mwfl::Command(kExit, L"Exit", [this] { Close(); }));
    }

    void BuildMenu() {
        mwfl::Menu bar;
        mwfl::Menu file;
        mwfl::Must(bar.Create(), "create workspace menu");
        mwfl::Must(file.CreatePopup(), "create workspace popup");
        file_menu_ = file.GetHandle();
        for (const auto id : {kNew, kOpen, kSave, kClose, kReopen,
                              kMove, kLeft, kRight})
            mwfl::Must(file.AppendCommand(*commands_.Find(id)), "append workspace command");
        mwfl::Must(file.AppendSeparator(), "append workspace separator");
        mwfl::Must(file.AppendCommand(*commands_.Find(kExit)), "append exit command");
        mwfl::Must(bar.AppendSubmenu(std::move(file), L"Document"), "append document menu");
        mwfl::Must(bar.AttachToWindow(GetHwnd()), "attach workspace menu");
    }

    HWND CreateEditor(mwfl::DocumentId id, std::wstring_view text) {
        const std::wstring terminated{text};
        return ::CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", terminated.c_str(),
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_MULTILINE | ES_AUTOVSCROLL |
                ES_AUTOHSCROLL | WS_VSCROLL | WS_HSCROLL,
            0, 0, 10, 10, GetHwnd(),
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(900 + id.value)),
            ::GetModuleHandleW(nullptr), nullptr);
    }

    void NewDocument(std::wstring title = {}, std::wstring text = {},
                     std::filesystem::path path = {},
                     std::optional<mwfl::FileStamp> stamp = {}) {
        const mwfl::DocumentId id{coordinator_.next_id++};
        if (title.empty()) title = L"Untitled " + std::to_wstring(id.value);
        HWND page = CreateEditor(id, text);
        mwfl::Must(page != nullptr, "create document editor");
        mwfl::Must(static_cast<bool>(model().Add({id, title, std::move(path)})),
                   "add document model");
        mwfl::Must(adapter_.BindPage(id, page) == mwfl::DocumentTabStatus::success,
                   "bind document editor");
        coordinator_.contents[id.value] = {std::move(text), stamp};
        model().Activate(id);
        mwfl::SetAccessibleName(page, title.c_str());
        Sync(L"Created " + title);
    }

    std::size_t RestoreSnapshot(const mwfl::WorkspaceSession& snapshot) {
        std::unordered_map<std::uint64_t, DocumentContent> restored_contents;
        const auto validator = [&](const mwfl::SessionDocument& item) {
            if (item.metadata.path.empty() || !item.metadata.path.is_absolute() ||
                !item.stamp) return mwfl::SessionDocumentDisposition::untrusted;
            const auto read = mwfl::ReadTextFile(item.metadata.path);
            if (read.status == mwfl::TextFileStatus::not_found)
                return mwfl::SessionDocumentDisposition::missing;
            if (!read.Succeeded() || read.value->stamp != *item.stamp)
                return mwfl::SessionDocumentDisposition::changed;
            restored_contents[item.metadata.id.value] =
                {read.value->text, read.value->stamp};
            return mwfl::SessionDocumentDisposition::restore;
        };
        const auto restored = mwfl::RestoreWorkspaceSession(
            model(), snapshot, validator,
            [&](const mwfl::SessionDocument& item) {
                return restored_contents.contains(item.metadata.id.value);
            });
        if (!restored) return 0;
        for (const auto& document : model().GetDocuments()) {
            auto content = restored_contents.find(document.id.value);
            if (content == restored_contents.end()) continue;
            HWND page = CreateEditor(document.id, content->second.text);
            if (!page || adapter_.BindPage(document.id, page) !=
                    mwfl::DocumentTabStatus::success)
                throw std::runtime_error("restore document page failed");
            coordinator_.contents[document.id.value] = std::move(content->second);
            mwfl::SetAccessibleName(page, document.title.c_str());
            coordinator_.next_id = (std::max)(coordinator_.next_id,
                                               document.id.value + 1);
        }
        for (const auto& recent : model().GetRecentlyClosed()) {
            auto content = restored_contents.find(recent.id.value);
            if (content != restored_contents.end())
                coordinator_.contents[recent.id.value] = std::move(content->second);
            coordinator_.next_id = (std::max)(coordinator_.next_id,
                                               recent.id.value + 1);
        }
        Sync(restored.issues.empty() ? L"Session restored"
                                     : L"Session restored with skipped files");
        return restored.restored_documents;
    }

    void OpenInteractive() {
        const auto selected = mwfl::ShowOpenFileDialog({
            .owner = GetHwnd(), .title = L"Open document",
            .filters = {{L"Text files", L"*.txt"}, {L"All files", L"*.*"}}});
        if (selected.accepted) OpenPath(selected.path);
    }

    bool OpenPath(const std::filesystem::path& path) {
        for (std::size_t workspace = 0; workspace < coordinator_.models.size(); ++workspace) {
            if (!coordinator_.models[workspace].ContainsPath(path)) continue;
            for (const auto& document : coordinator_.models[workspace].GetDocuments()) {
                if (!SamePath(document.path, path)) continue;
                coordinator_.models[workspace].Activate(document.id);
                coordinator_.windows[workspace]->Sync(L"Already open");
                ::SetForegroundWindow(coordinator_.windows[workspace]->GetHwnd());
                return true;
            }
        }
        const auto read = mwfl::ReadTextFile(path);
        if (!read.Succeeded()) return false;
        NewDocument(path.filename().wstring(), read.value->text, path, read.value->stamp);
        Sync(L"Opened file");
        return true;
    }

    bool ReopenClosed() {
        if (model().GetRecentlyClosed().empty()) return false;
        const auto metadata = model().GetRecentlyClosed().front();
        auto content = coordinator_.contents.find(metadata.id.value);
        if (content == coordinator_.contents.end()) {
            if (metadata.path.empty()) return false;
            const auto read = mwfl::ReadTextFile(metadata.path);
            if (!read.Succeeded()) return false;
            content = coordinator_.contents.emplace(metadata.id.value,
                DocumentContent{read.value->text, read.value->stamp}).first;
        }
        HWND page = CreateEditor(metadata.id, content->second.text);
        if (!page) return false;
        if (!model().ReopenRecentlyClosed()) {
            ::DestroyWindow(page);
            return false;
        }
        if (adapter_.BindPage(metadata.id, page) != mwfl::DocumentTabStatus::success) {
            ::DestroyWindow(page);
            return false;
        }
        mwfl::SetAccessibleName(page, metadata.title.c_str());
        Sync(L"Reopened closed document");
        return true;
    }

    bool SaveActive() {
        const auto id = model().GetActiveId();
        if (!id) return false;
        const auto* document = model().Find(*id);
        const std::wstring title = document->title;
        std::filesystem::path path = document->path;
        if (path.empty()) {
            if (coordinator_.self_test) {
                path = std::filesystem::temp_directory_path() /
                    (L"mwfl-workspace-" + std::to_wstring(id->value) + L".txt");
            } else {
                const auto selected = mwfl::ShowSaveFileDialog({
                    .owner = GetHwnd(), .title = L"Save document",
                    .filters = {{L"Text files", L"*.txt"}, {L"All files", L"*.*"}},
                    .default_extension = L"txt", .path_must_exist = false});
                if (!selected.accepted) return false;
                path = selected.path;
            }
        }
        auto& content = coordinator_.contents[id->value];
        const auto saved = mwfl::WriteTextFileAtomic(path, content.text,
                                                      mwfl::TextEncoding::utf8,
                                                      content.stamp);
        if (!saved.Succeeded()) return false;
        content.stamp = saved.stamp;
        model().Rename(*id, title, path);
        model().SetDirty(*id, false);
        Sync(L"Saved");
        return true;
    }

    bool CloseActive(bool discard = false) {
        const auto id = model().GetActiveId();
        if (!id) return false;
        const auto* document = model().Find(*id);
        if (document->dirty && !discard && !coordinator_.self_test) {
            const int answer = ::MessageBoxW(GetHwnd(), L"Save changes before closing?",
                L"mwfl Documents", MB_YESNOCANCEL | MB_ICONWARNING);
            if (answer == IDCANCEL) return false;
            if (answer == IDYES && !SaveActive()) return false;
        }
        const HWND page = adapter_.FindPage(*id);
        if (!model().Close(*id) ||
            adapter_.UnbindPage(*id) != mwfl::DocumentTabStatus::success) return false;
        if (page) ::DestroyWindow(page);
        Sync(L"Closed");
        return true;
    }

    void Reorder(int delta) {
        const auto id = model().GetActiveId();
        if (!id) return;
        const auto index = model().FindIndex(*id);
        if (!index) return;
        const auto next = static_cast<std::ptrdiff_t>(*index) + delta;
        if (next < 0 || next >= static_cast<std::ptrdiff_t>(model().GetCount())) return;
        model().Move(*id, static_cast<std::size_t>(next));
        Sync(L"Reordered");
    }

    bool ConfirmCloseAll() {
        std::vector<mwfl::DocumentCloseDecision> decisions;
        for (const auto& document : model().GetDocuments()) {
            if (document.dirty)
                decisions.push_back({document.id, coordinator_.self_test
                    ? mwfl::DocumentCloseChoice::discard
                    : mwfl::DocumentCloseChoice::cancel});
        }
        if (!coordinator_.self_test && !decisions.empty()) {
            const int answer = ::MessageBoxW(GetHwnd(),
                L"Discard all unsaved changes and close this workspace?",
                L"mwfl Documents", MB_OKCANCEL | MB_ICONWARNING);
            if (answer != IDOK) return false;
            for (auto& decision : decisions)
                decision.choice = mwfl::DocumentCloseChoice::discard;
        }
        const auto plan = mwfl::BuildCoordinatedClosePlan(model(), decisions);
        return static_cast<bool>(mwfl::ExecuteCoordinatedClose(model(), plan, {}));
    }

    HWND ActivePage() const {
        const auto id = coordinator_.models[index_].GetActiveId();
        return id ? adapter_.FindPage(*id) : nullptr;
    }

    static std::wstring GetText(HWND edit) {
        const int length = ::GetWindowTextLengthW(edit);
        std::wstring value(static_cast<std::size_t>(length) + 1, L'\0');
        ::GetWindowTextW(edit, value.data(), length + 1);
        value.resize(static_cast<std::size_t>(length));
        return value;
    }

    void RunSelfTest() {
        if (!coordinator_.windows[1] || model().GetCount() != 2 ||
            coordinator_.models[1].GetCount() != 1)
            throw std::runtime_error("initial two-window state failed");
        const auto active = model().GetActiveId();
        if (!active) throw std::runtime_error("missing active document");
        const HWND active_page = ActivePage();
        ::SetWindowTextW(active_page, L"self-test changed text");
        ::SendMessageW(GetHwnd(), WM_COMMAND,
            MAKEWPARAM(static_cast<WORD>(::GetDlgCtrlID(active_page)), EN_CHANGE),
            reinterpret_cast<LPARAM>(active_page));
        if (!model().Find(*active)->dirty) throw std::runtime_error("dirty routing failed");
        if (!SaveActive()) throw std::runtime_error("active save failed");
        const auto saved_path = model().Find(*active)->path;
        if (!CloseActive(true) || !ReopenClosed())
            throw std::runtime_error("recently closed reopen failed");
        const auto reopened = model().GetActiveId();
        if (!reopened || *reopened != *active)
            throw std::runtime_error("recent identity was not preserved");
        if (!CloseActive(true)) throw std::runtime_error("second close failed");
        model().ClearRecentlyClosed();
        coordinator_.contents.erase(active->value);
        if (!OpenPath(saved_path)) throw std::runtime_error("file reopen failed");
        if (!mwfl::WriteTextFileAtomic(saved_path, L"external change").Succeeded())
            throw std::runtime_error("external change setup failed");
        const HWND reopened_page = ActivePage();
        ::SetWindowTextW(reopened_page, L"local unsaved change");
        ::SendMessageW(GetHwnd(), WM_COMMAND,
            MAKEWPARAM(static_cast<WORD>(::GetDlgCtrlID(reopened_page)), EN_CHANGE),
            reinterpret_cast<LPARAM>(reopened_page));
        if (SaveActive() || !model().Find(*model().GetActiveId())->dirty)
            throw std::runtime_error("external change protection failed");
        const auto current_disk = mwfl::ReadTextFile(saved_path);
        if (!current_disk.Succeeded())
            throw std::runtime_error("read changed file failed");
        coordinator_.contents[model().GetActiveId()->value].stamp =
            current_disk.value->stamp;
        if (!SaveActive()) throw std::runtime_error("reconciled save failed");
        Reorder(-1);
        if (!coordinator_.MoveActive(0) || coordinator_.models[1].GetCount() != 2)
            throw std::runtime_error("cross-window transfer failed");
        if (!coordinator_.SaveSession())
            throw std::runtime_error("session persistence failed");
        const auto loaded = mwfl::LoadDocumentSession(coordinator_.session_path);
        if (!loaded || loaded.session->workspaces.size() != 2)
            throw std::runtime_error("saved application session failed to load");
        mwfl::DocumentWorkspaceModel restored_a{{1}, 8};
        mwfl::DocumentWorkspaceModel restored_b{{2}, 8};
        const auto validator = [](const mwfl::SessionDocument& item) {
            if (item.metadata.path.empty() || !item.metadata.path.is_absolute() ||
                !item.stamp) return mwfl::SessionDocumentDisposition::untrusted;
            const auto read = mwfl::ReadTextFile(item.metadata.path);
            if (read.status == mwfl::TextFileStatus::not_found)
                return mwfl::SessionDocumentDisposition::missing;
            if (!read.Succeeded() || read.value->stamp != *item.stamp)
                return mwfl::SessionDocumentDisposition::changed;
            return mwfl::SessionDocumentDisposition::restore;
        };
        const auto restore = [](const mwfl::SessionDocument&) { return true; };
        if (!mwfl::RestoreWorkspaceSession(
                restored_a, loaded.session->workspaces[0], validator, restore) ||
            !mwfl::RestoreWorkspaceSession(
                restored_b, loaded.session->workspaces[1], validator, restore) ||
            restored_a.GetCount() != 0 || restored_b.GetCount() != 1)
            throw std::runtime_error("session restore failed");
        if (!mwfl::WriteTextFileAtomic(saved_path, L"changed after session").Succeeded())
            throw std::runtime_error("changed restore setup failed");
        mwfl::DocumentWorkspaceModel changed_restore{{2}, 8};
        const auto changed_result = mwfl::RestoreWorkspaceSession(
            changed_restore, loaded.session->workspaces[1], validator, restore);
        if (!changed_result || changed_result.issues.empty() ||
            changed_result.issues.front().disposition !=
                mwfl::SessionDocumentDisposition::changed ||
            changed_restore.GetCount() != 0)
            throw std::runtime_error("changed session path was not skipped");
        std::error_code ignored;
        std::filesystem::remove(saved_path, ignored);
        mwfl::DocumentWorkspaceModel missing_restore{{2}, 8};
        const auto missing_result = mwfl::RestoreWorkspaceSession(
            missing_restore, loaded.session->workspaces[1], validator, restore);
        if (!missing_result || missing_result.issues.empty() ||
            missing_result.issues.front().disposition !=
                mwfl::SessionDocumentDisposition::missing)
            throw std::runtime_error("missing session path was not skipped");
        auto untrusted_snapshot = loaded.session->workspaces[1];
        untrusted_snapshot.documents.front().metadata.path = L"relative.txt";
        mwfl::DocumentWorkspaceModel untrusted_restore{{2}, 8};
        const auto untrusted_result = mwfl::RestoreWorkspaceSession(
            untrusted_restore, untrusted_snapshot, validator, restore);
        if (!untrusted_result || untrusted_result.issues.empty() ||
            untrusted_result.issues.front().disposition !=
                mwfl::SessionDocumentDisposition::untrusted)
            throw std::runtime_error("untrusted session path was not skipped");
        std::filesystem::remove(coordinator_.session_path, ignored);
    }

    Coordinator& coordinator_;
    std::size_t index_ = 0;
    bool primary_ = false;
    mwfl::CommandSet commands_;
    mwfl::AcceleratorTable accelerators_;
    HMENU file_menu_ = nullptr;
    mwfl::Toolbar toolbar_;
    mwfl::TabControl tabs_;
    mwfl::StatusBar status_;
    mwfl::DocumentTabWorkspaceAdapter adapter_;
};

void Coordinator::EnsureSecondary() {
    if (secondary) return;
    secondary = std::make_unique<WorkspaceWindow>(*this, 1, false);
    mwfl::WindowOptions options;
    options.quit_on_destroy = false;
    secondary->ConfigureWindowOptions(options);
    RECT bounds{650, 120, 1200, 600};
    if (!secondary->Create(nullptr, bounds, L"Workspace B",
                           WS_OVERLAPPEDWINDOW, 0))
        throw std::runtime_error("create secondary workspace failed");
    if (!self_test) ::ShowWindow(secondary->GetHwnd(), SW_SHOW);
}

bool Coordinator::MoveActive(std::size_t source) {
    const std::size_t destination = 1 - source;
    auto* from = windows[source];
    auto* to = windows[destination];
    if (!from || !to) return false;
    const auto id = models[source].GetActiveId();
    if (!id) return false;
    const auto moved = mwfl::TransferDocumentWithPage(
        models[source], from->adapter(), models[destination], to->adapter(), *id);
    if (!moved) return false;
    from->Sync(L"Moved document out");
    to->Sync(L"Received document");
    return true;
}

void Coordinator::InitializeWorkspaces() {
    std::size_t restored = 0;
    if (pending_session) {
        for (std::size_t index = 0; index < models.size(); ++index) {
            const auto found = std::ranges::find(
                pending_session->workspaces, models[index].GetId(),
                &mwfl::WorkspaceSession::workspace);
            if (found != pending_session->workspaces.end())
                restored += windows[index]->RestoreSnapshot(*found);
        }
        pending_session.reset();
    }
    if (restored == 0) {
        windows[0]->NewDocument(
            L"Welcome", L"This is a real multi-document workspace.\r\n");
        windows[0]->NewDocument(
            L"Notes", L"Edit, save, reorder, close, or move this tab.\r\n");
        windows[1]->NewDocument(
            L"Second window", L"Documents can move between windows.\r\n");
    }
}

bool Coordinator::SaveSession() {
    if (session_path.empty()) return false;
    mwfl::DocumentSession session;
    for (const auto& workspace : models) {
        auto snapshot = mwfl::CaptureWorkspaceSession(workspace);
        const auto decorate = [&](std::vector<mwfl::SessionDocument>& documents) {
            std::erase_if(documents, [&](mwfl::SessionDocument& item) {
                const auto content = contents.find(item.metadata.id.value);
                if (item.metadata.path.empty() || content == contents.end() ||
                    !content->second.stamp) return true;
                item.stamp = content->second.stamp;
                // Exit decisions discard unsaved text. The next session loads
                // the stamped on-disk version, not an unavailable dirty buffer.
                item.metadata.dirty = false;
                item.metadata.can_undo = false;
                item.metadata.can_redo = false;
                return false;
            });
        };
        decorate(snapshot.documents);
        decorate(snapshot.recently_closed);
        if (snapshot.active && std::ranges::none_of(snapshot.documents,
                [&](const auto& item) { return item.metadata.id == snapshot.active; }))
            snapshot.active.reset();
        session.workspaces.push_back(std::move(snapshot));
    }
    std::error_code error;
    std::filesystem::create_directories(session_path.parent_path(), error);
    if (error) return false;
    return mwfl::SaveDocumentSessionAtomic(session_path, session) ==
           mwfl::DocumentSessionStatus::success;
}

bool Coordinator::ConfirmShutdown(HWND owner) {
    bool dirty = false;
    for (const auto& workspace : models)
        for (const auto& document : workspace.GetDocuments()) dirty |= document.dirty;
    if (dirty && !self_test && ::MessageBoxW(owner,
            L"Discard all unsaved changes in both workspace windows?",
            L"mwfl Documents", MB_OKCANCEL | MB_ICONWARNING) != IDOK)
        return false;
    if (!SaveSession()) {
        if (self_test) return false;
        if (::MessageBoxW(owner,
                L"The workspace session could not be saved. Close anyway?",
                L"mwfl Documents", MB_OKCANCEL | MB_ICONWARNING) != IDOK)
            return false;
    }
    std::array<mwfl::CoordinatedClosePlan, 2> plans;
    for (std::size_t index = 0; index < models.size(); ++index) {
        std::vector<mwfl::DocumentCloseDecision> decisions;
        for (const auto& document : models[index].GetDocuments())
            if (document.dirty)
                decisions.push_back(
                    {document.id, mwfl::DocumentCloseChoice::discard});
        plans[index] = mwfl::BuildCoordinatedClosePlan(models[index], decisions);
        if (!plans[index]) return false;
    }
    for (std::size_t index = 0; index < models.size(); ++index)
        if (!mwfl::ExecuteCoordinatedClose(models[index], plans[index], {})) return false;
    return true;
}

}  // namespace

int RunDocumentWorkspaceExample(HINSTANCE instance, int show) {
    int count = 0;
    wchar_t** arguments = ::CommandLineToArgvW(::GetCommandLineW(), &count);
    bool self_test = false;
    std::optional<std::filesystem::path> result;
    for (int index = 1; arguments && index < count; ++index) {
        if (std::wstring_view{arguments[index]} == L"--self-test") self_test = true;
        else if (std::wstring_view{arguments[index]} == L"--self-test-result" &&
                 index + 1 < count) result = arguments[++index];
    }
    if (arguments) ::LocalFree(arguments);
    Coordinator coordinator{self_test, std::move(result)};
    return mwfl::RunApplication<WorkspaceWindow>(instance, self_test ? SW_HIDE : show,
        {.title = L"Workspace A — mwfl Documents",
         .initial_bounds = {{}, {560.0_dip, 480.0_dip}},
         .use_default_bounds = false}, {}, coordinator, 0, true);
}
