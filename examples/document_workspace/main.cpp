#include <mwtl/mwtl.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>

using mwtl::operator""_dip;

namespace {

constexpr mwtl::ControlId kNew{800};
constexpr mwtl::ControlId kSave{801};
constexpr mwtl::ControlId kClose{802};
constexpr mwtl::ControlId kMove{803};
constexpr mwtl::ControlId kLeft{804};
constexpr mwtl::ControlId kRight{805};
constexpr mwtl::ControlId kUndo{806};
constexpr mwtl::ControlId kRedo{807};
constexpr mwtl::ControlId kExit{808};
constexpr mwtl::ControlId kOpen{809};
constexpr mwtl::ControlId kReopen{810};
constexpr mwtl::ControlId kTabs{820};
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
    std::optional<mwtl::FileStamp> stamp;
};

struct Coordinator {
    explicit Coordinator(bool test, std::optional<std::filesystem::path> result)
        : self_test(test), result_path(std::move(result)) {}

    std::array<mwtl::DocumentWorkspaceModel, 2> models{
        mwtl::DocumentWorkspaceModel{{1}, 8},
        mwtl::DocumentWorkspaceModel{{2}, 8}};
    std::unordered_map<std::uint64_t, DocumentContent> contents;
    std::array<WorkspaceWindow*, 2> windows{};
    std::unique_ptr<WorkspaceWindow> secondary;
    std::uint64_t next_id = 1;
    bool self_test = false;
    std::optional<std::filesystem::path> result_path;

    void EnsureSecondary();
    bool MoveActive(std::size_t source);
    bool ConfirmShutdown(HWND owner);
    void Report(std::string_view value) const {
        if (result_path) {
            std::ofstream output(*result_path, std::ios::binary | std::ios::trunc);
            output << value;
        }
    }
};

class WorkspaceWindow final : public mwtl::WindowBase {
public:
    WorkspaceWindow(Coordinator& coordinator, std::size_t index, bool primary)
        : coordinator_(coordinator), index_(index), primary_(primary) {}

    void BuildUI() override {
        coordinator_.windows[index_] = this;
        BuildCommands();
        mwtl::ControlHost ui{*this};
        ui.Add(toolbar_);
        ui.Add(tabs_, mwtl::TabControlOptions{});
        ui.Add(status_);
        for (const auto& command : commands_.GetCommands()) {
            if (command.GetId() != kExit)
                mwtl::Must(toolbar_.AddCommand(command), "add workspace toolbar command");
        }
        toolbar_.AutoSize();
        mwtl::Must(adapter_.Attach(tabs_) == mwtl::DocumentTabStatus::success,
                   "attach document tabs");
        BuildMenu();
        mwtl::Must(accelerators_.Create(commands_), "create workspace accelerators");
        SetAccelerators(accelerators_.GetHandle());
        mwtl::Must(mwtl::SetAccessibleName(tabs_.GetHwnd(), L"Open documents"),
                   "name document tabs");
        mwtl::Must(mwtl::SetAccessibleName(status_.GetHwnd(), L"Workspace status"),
                   "name workspace status");
        SetLayout(mwtl::Column()
            .Add(toolbar_, mwtl::Auto())
            .Add(tabs_, mwtl::Stretch())
            .Add(status_, mwtl::Fixed(26.0_dip)));
        if (index_ == 0) {
            NewDocument(L"Welcome", L"This is a real multi-document workspace.\r\n");
            NewDocument(L"Notes", L"Edit, save, reorder, close, or move this tab.\r\n");
            coordinator_.EnsureSecondary();
            if (coordinator_.self_test &&
                !::PostMessageW(GetHwnd(), kRunSelfTest, 0, 0))
                throw std::runtime_error("post workspace self-test failed");
        } else {
            NewDocument(L"Second window", L"Documents can move between windows.\r\n");
        }
        Sync(L"Ready");
    }

    mwtl::EventResult OnCommand(const mwtl::CommandEvent& event) override {
        if (event.notification == EN_CHANGE && event.control) {
            for (const auto& binding : adapter_.GetPages()) {
                if (binding.page != event.control) continue;
                coordinator_.contents[binding.document.value].text = GetText(binding.page);
                model().SetDirty(binding.document, true);
                model().SetUndoState(binding.document,
                    ::SendMessageW(binding.page, EM_CANUNDO, 0, 0) != 0, false);
                Sync(L"Modified");
                return mwtl::EventResult::Handled();
            }
        }
        return commands_.Dispatch(event);
    }

    mwtl::EventResult OnNotify(const mwtl::NotifyEvent& event) override {
        if (event.Is(tabs_, TCN_SELCHANGE)) {
            adapter_.ActivateNativeSelection(model());
            Sync(L"Active document changed");
            return mwtl::EventResult::Handled();
        }
        return mwtl::EventResult::Propagate();
    }

    mwtl::EventResult OnResize(const mwtl::ResizeEvent&) override {
        adapter_.ArrangePages();
        return mwtl::EventResult::Propagate();
    }

    mwtl::EventResult OnMessage(const mwtl::WindowMessage& event) override {
        if (event.id != kRunSelfTest) return mwtl::EventResult::Propagate();
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
        return mwtl::EventResult::Handled();
    }

    mwtl::EventResult OnClose() override {
        if (primary_) {
            if (!coordinator_.ConfirmShutdown(GetHwnd()))
                return mwtl::EventResult::Handled();
        } else if (!ConfirmCloseAll()) {
            return mwtl::EventResult::Handled();
        }
        if (primary_ && coordinator_.secondary && coordinator_.secondary->IsWindow())
            ::DestroyWindow(coordinator_.secondary->GetHwnd());
        coordinator_.windows[index_] = nullptr;
        return mwtl::EventResult::Propagate();
    }

    mwtl::DocumentWorkspaceModel& model() { return coordinator_.models[index_]; }
    mwtl::DocumentTabWorkspaceAdapter& adapter() { return adapter_; }

    void Sync(std::wstring_view message) {
        const auto projection = mwtl::BuildActiveDocumentCommandProjection(model());
        if (mwtl::ApplyActiveDocumentCommandProjection(
                commands_, {kSave, kClose, kUndo, kRedo}, projection) !=
            mwtl::DocumentCommandProjectionStatus::success)
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
                 std::wstring{L" — mwtl Documents"});
        ::DrawMenuBar(GetHwnd());
    }

private:
    void BuildCommands() {
        commands_
            .Add(mwtl::Command(kNew, L"New", [this] { NewDocument(); })
                     .SetShortcut({FVIRTKEY | FCONTROL, 'N'}))
            .Add(mwtl::Command(kOpen, L"Open…", [this] { OpenInteractive(); })
                     .SetShortcut({FVIRTKEY | FCONTROL, 'O'}))
            .Add(mwtl::Command(kSave, L"Save", [this] { SaveActive(); })
                     .SetShortcut({FVIRTKEY | FCONTROL, 'S'}))
            .Add(mwtl::Command(kClose, L"Close", [this] { CloseActive(); })
                     .SetShortcut({FVIRTKEY | FCONTROL, 'W'}))
            .Add(mwtl::Command(kMove, L"Move to other window",
                               [this] { coordinator_.MoveActive(index_); }))
            .Add(mwtl::Command(kLeft, L"Move tab left", [this] { Reorder(-1); }))
            .Add(mwtl::Command(kRight, L"Move tab right", [this] { Reorder(1); }))
            .Add(mwtl::Command(kReopen, L"Reopen closed", [this] { ReopenClosed(); })
                     .SetShortcut({FVIRTKEY | FCONTROL | FSHIFT, 'T'}))
            .Add(mwtl::Command(kUndo, L"Undo", [this] {
                if (const HWND page = ActivePage()) ::SendMessageW(page, WM_UNDO, 0, 0);
            }).SetShortcut({FVIRTKEY | FCONTROL, 'Z'}))
            .Add(mwtl::Command(kRedo, L"Redo", [] {}).SetEnabled(false))
            .Add(mwtl::Command(kExit, L"Exit", [this] { Close(); }));
    }

    void BuildMenu() {
        mwtl::Menu bar;
        mwtl::Menu file;
        mwtl::Must(bar.Create(), "create workspace menu");
        mwtl::Must(file.CreatePopup(), "create workspace popup");
        file_menu_ = file.GetHandle();
        for (const auto id : {kNew, kOpen, kSave, kClose, kReopen,
                              kMove, kLeft, kRight})
            mwtl::Must(file.AppendCommand(*commands_.Find(id)), "append workspace command");
        mwtl::Must(file.AppendSeparator(), "append workspace separator");
        mwtl::Must(file.AppendCommand(*commands_.Find(kExit)), "append exit command");
        mwtl::Must(bar.AppendSubmenu(std::move(file), L"Document"), "append document menu");
        mwtl::Must(bar.AttachToWindow(GetHwnd()), "attach workspace menu");
    }

    HWND CreateEditor(mwtl::DocumentId id, std::wstring_view text) {
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
                     std::optional<mwtl::FileStamp> stamp = {}) {
        const mwtl::DocumentId id{coordinator_.next_id++};
        if (title.empty()) title = L"Untitled " + std::to_wstring(id.value);
        HWND page = CreateEditor(id, text);
        mwtl::Must(page != nullptr, "create document editor");
        mwtl::Must(static_cast<bool>(model().Add({id, title, std::move(path)})),
                   "add document model");
        mwtl::Must(adapter_.BindPage(id, page) == mwtl::DocumentTabStatus::success,
                   "bind document editor");
        coordinator_.contents[id.value] = {std::move(text), stamp};
        model().Activate(id);
        mwtl::SetAccessibleName(page, title.c_str());
        Sync(L"Created " + title);
    }

    void OpenInteractive() {
        const auto selected = mwtl::ShowOpenFileDialog({
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
        const auto read = mwtl::ReadTextFile(path);
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
            const auto read = mwtl::ReadTextFile(metadata.path);
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
        if (adapter_.BindPage(metadata.id, page) != mwtl::DocumentTabStatus::success) {
            ::DestroyWindow(page);
            return false;
        }
        mwtl::SetAccessibleName(page, metadata.title.c_str());
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
                    (L"mwtl-workspace-" + std::to_wstring(id->value) + L".txt");
            } else {
                const auto selected = mwtl::ShowSaveFileDialog({
                    .owner = GetHwnd(), .title = L"Save document",
                    .filters = {{L"Text files", L"*.txt"}, {L"All files", L"*.*"}},
                    .default_extension = L"txt", .path_must_exist = false});
                if (!selected.accepted) return false;
                path = selected.path;
            }
        }
        auto& content = coordinator_.contents[id->value];
        const auto saved = mwtl::WriteTextFileAtomic(path, content.text,
                                                      mwtl::TextEncoding::utf8,
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
                L"mwtl Documents", MB_YESNOCANCEL | MB_ICONWARNING);
            if (answer == IDCANCEL) return false;
            if (answer == IDYES && !SaveActive()) return false;
        }
        const HWND page = adapter_.FindPage(*id);
        if (!model().Close(*id) ||
            adapter_.UnbindPage(*id) != mwtl::DocumentTabStatus::success) return false;
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
        std::vector<mwtl::DocumentCloseDecision> decisions;
        for (const auto& document : model().GetDocuments()) {
            if (document.dirty)
                decisions.push_back({document.id, coordinator_.self_test
                    ? mwtl::DocumentCloseChoice::discard
                    : mwtl::DocumentCloseChoice::cancel});
        }
        if (!coordinator_.self_test && !decisions.empty()) {
            const int answer = ::MessageBoxW(GetHwnd(),
                L"Discard all unsaved changes and close this workspace?",
                L"mwtl Documents", MB_OKCANCEL | MB_ICONWARNING);
            if (answer != IDOK) return false;
            for (auto& decision : decisions)
                decision.choice = mwtl::DocumentCloseChoice::discard;
        }
        const auto plan = mwtl::BuildCoordinatedClosePlan(model(), decisions);
        return static_cast<bool>(mwtl::ExecuteCoordinatedClose(model(), plan, {}));
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
        if (!mwtl::WriteTextFileAtomic(saved_path, L"external change").Succeeded())
            throw std::runtime_error("external change setup failed");
        const HWND reopened_page = ActivePage();
        ::SetWindowTextW(reopened_page, L"local unsaved change");
        ::SendMessageW(GetHwnd(), WM_COMMAND,
            MAKEWPARAM(static_cast<WORD>(::GetDlgCtrlID(reopened_page)), EN_CHANGE),
            reinterpret_cast<LPARAM>(reopened_page));
        if (SaveActive() || !model().Find(*model().GetActiveId())->dirty)
            throw std::runtime_error("external change protection failed");
        Reorder(-1);
        if (!coordinator_.MoveActive(0) || coordinator_.models[1].GetCount() != 2)
            throw std::runtime_error("cross-window transfer failed");
        mwtl::DocumentSession session;
        session.workspaces.push_back(mwtl::CaptureWorkspaceSession(coordinator_.models[0]));
        session.workspaces.push_back(mwtl::CaptureWorkspaceSession(coordinator_.models[1]));
        const auto path = std::filesystem::temp_directory_path() /
            (L"mwtl-document-workspace-session-" +
             std::to_wstring(::GetCurrentProcessId()) + L".state");
        if (mwtl::SaveDocumentSessionAtomic(path, session) !=
                mwtl::DocumentSessionStatus::success ||
            !mwtl::LoadDocumentSession(path))
            throw std::runtime_error("session persistence failed");
        const auto loaded = mwtl::LoadDocumentSession(path);
        mwtl::DocumentWorkspaceModel restored_a{{1}, 8};
        mwtl::DocumentWorkspaceModel restored_b{{2}, 8};
        const auto validator = [](const mwtl::SessionDocument&) {
            return mwtl::SessionDocumentDisposition::restore;
        };
        const auto restore = [](const mwtl::SessionDocument&) { return true; };
        if (!mwtl::RestoreWorkspaceSession(
                restored_a, loaded.session->workspaces[0], validator, restore) ||
            !mwtl::RestoreWorkspaceSession(
                restored_b, loaded.session->workspaces[1], validator, restore) ||
            restored_a.GetCount() != coordinator_.models[0].GetCount() ||
            restored_b.GetCount() != coordinator_.models[1].GetCount())
            throw std::runtime_error("session restore failed");
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
        std::filesystem::remove(saved_path, ignored);
    }

    Coordinator& coordinator_;
    std::size_t index_ = 0;
    bool primary_ = false;
    mwtl::CommandSet commands_;
    mwtl::AcceleratorTable accelerators_;
    HMENU file_menu_ = nullptr;
    mwtl::Toolbar toolbar_;
    mwtl::TabControl tabs_;
    mwtl::StatusBar status_;
    mwtl::DocumentTabWorkspaceAdapter adapter_;
};

void Coordinator::EnsureSecondary() {
    if (secondary) return;
    secondary = std::make_unique<WorkspaceWindow>(*this, 1, false);
    mwtl::WindowOptions options;
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
    const auto moved = mwtl::TransferDocumentWithPage(
        models[source], from->adapter(), models[destination], to->adapter(), *id);
    if (!moved) return false;
    from->Sync(L"Moved document out");
    to->Sync(L"Received document");
    return true;
}

bool Coordinator::ConfirmShutdown(HWND owner) {
    bool dirty = false;
    for (const auto& workspace : models)
        for (const auto& document : workspace.GetDocuments()) dirty |= document.dirty;
    if (dirty && !self_test && ::MessageBoxW(owner,
            L"Discard all unsaved changes in both workspace windows?",
            L"mwtl Documents", MB_OKCANCEL | MB_ICONWARNING) != IDOK)
        return false;
    std::array<mwtl::CoordinatedClosePlan, 2> plans;
    for (std::size_t index = 0; index < models.size(); ++index) {
        std::vector<mwtl::DocumentCloseDecision> decisions;
        for (const auto& document : models[index].GetDocuments())
            if (document.dirty)
                decisions.push_back(
                    {document.id, mwtl::DocumentCloseChoice::discard});
        plans[index] = mwtl::BuildCoordinatedClosePlan(models[index], decisions);
        if (!plans[index]) return false;
    }
    for (std::size_t index = 0; index < models.size(); ++index)
        if (!mwtl::ExecuteCoordinatedClose(models[index], plans[index], {})) return false;
    return true;
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
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
    return mwtl::RunApplication<WorkspaceWindow>(instance, self_test ? SW_HIDE : show,
        {.title = L"Workspace A — mwtl Documents",
         .initial_bounds = {{}, {560.0_dip, 480.0_dip}},
         .use_default_bounds = false}, {}, coordinator, 0, true);
}
