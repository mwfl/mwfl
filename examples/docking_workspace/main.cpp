#include <mwtl/mwtl.h>

#include <commctrl.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <stdexcept>
#include <string_view>
#include <vector>

using mwtl::operator""_dip;

namespace {

constexpr mwtl::ControlId kFloatOutput{900};
constexpr mwtl::ControlId kDockOutput{901};
constexpr mwtl::ControlId kAutoHideExplorer{902};
constexpr mwtl::ControlId kPinExplorer{903};
constexpr mwtl::ControlId kKeyboardDock{904};
constexpr mwtl::ControlId kSaveLayout{905};
constexpr mwtl::ControlId kResetLayout{906};
constexpr mwtl::ControlId kExit{907};
constexpr mwtl::ControlId kToolbar{910};
constexpr mwtl::ControlId kStatus{911};
constexpr mwtl::ControlId kOuterSplitter{912};
constexpr mwtl::ControlId kInnerSplitter{913};
constexpr UINT kRunSelfTest = WM_APP + 0x180;

constexpr mwtl::DockPanelId kMainDocument{1};
constexpr mwtl::DockPanelId kReadmeDocument{2};
constexpr mwtl::DockPanelId kSolutionExplorer{3};
constexpr mwtl::DockPanelId kOutputPanel{4};
constexpr mwtl::DockGroupId kDocuments{10};
constexpr mwtl::DockGroupId kLeftTools{20};
constexpr mwtl::DockGroupId kBottomTools{30};
constexpr mwtl::DockGroupId kFloatingTools{31};

bool g_self_test = false;

HWND CreatePane(HWND parent, int id, const wchar_t* name) {
    return ::CreateWindowExW(WS_EX_CONTROLPARENT, L"STATIC", name,
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        0, 0, 10, 10, parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        ::GetModuleHandleW(nullptr), nullptr);
}

class GroupView final {
public:
    bool Create(HWND parent, HWND event_target, int id,
                mwtl::DockGroupId group, const wchar_t* accessible_name) {
        event_target_ = event_target;
        group_ = group;
        host_ = CreatePane(parent, id, accessible_name);
        if (!host_ || !tabs_.Create(host_, {id + 100},
                {0.0_dip, 0.0_dip, 100.0_dip, 28.0_dip}) ||
            !mwtl::SetAccessibleName(host_, accessible_name) ||
            !mwtl::SetAccessibleName(tabs_.GetHwnd(), accessible_name) ||
            !::SetWindowSubclass(host_, Procedure,
                static_cast<UINT_PTR>(id), reinterpret_cast<DWORD_PTR>(this)))
            return false;
        return true;
    }

    void RegisterPanel(mwtl::DockPanelId id, HWND window) {
        panels_.push_back({id, window});
    }

    void Synchronize(const mwtl::DockLayoutSnapshot& snapshot) {
        for (const auto& panel : panels_)
            tabs_.RemoveTab(mwtl::TabId{panel.first.value});
        const auto found = std::find_if(snapshot.groups.begin(), snapshot.groups.end(),
            [&](const mwtl::DockGroup& group) { return group.id == group_; });
        if (found != snapshot.groups.end()) {
            for (mwtl::DockPanelId id : found->panels) {
                const auto panel = std::find_if(snapshot.panels.begin(), snapshot.panels.end(),
                    [&](const mwtl::DockPanel& value) { return value.id == id; });
                if (panel != snapshot.panels.end())
                    tabs_.AddTab(mwtl::TabId{id.value}, panel->title);
            }
            if (found->active) tabs_.SetSelection(mwtl::TabId{found->active->value});
        }
        Arrange();
    }

    void Arrange() noexcept {
        if (!host_) return;
        RECT client{};
        ::GetClientRect(host_, &client);
        const int tab_height = 29;
        const int client_width = static_cast<int>(client.right);
        const int client_height = static_cast<int>(client.bottom);
        ::SetWindowPos(tabs_.GetHwnd(), nullptr, 0, 0, client_width,
                       (std::min)(tab_height, client_height),
                       SWP_NOZORDER | SWP_NOACTIVATE);
        for (const auto& panel : panels_) {
            if (::IsWindow(panel.second) && ::GetParent(panel.second) == host_)
                ::SetWindowPos(panel.second, nullptr, 0, tab_height, client_width,
                    (std::max)(0, client_height - tab_height),
                    SWP_NOZORDER | SWP_NOACTIVATE);
        }
    }

    HWND GetHwnd() const noexcept { return host_; }
    mwtl::TabControl& Tabs() noexcept { return tabs_; }
    mwtl::DockGroupId GetGroup() const noexcept { return group_; }

private:
    static LRESULT CALLBACK Procedure(HWND window, UINT message, WPARAM wparam,
                                      LPARAM lparam, UINT_PTR subclass_id,
                                      DWORD_PTR reference) noexcept {
        auto* self = reinterpret_cast<GroupView*>(reference);
        if (message == WM_SIZE && self) self->Arrange();
        if ((message == WM_NOTIFY || message == WM_COMMAND ||
             message == WM_CONTEXTMENU) && self && self->event_target_)
            return ::SendMessageW(self->event_target_, message, wparam, lparam);
        if (message == WM_NCDESTROY) {
            ::RemoveWindowSubclass(window, Procedure, subclass_id);
            if (self) self->host_ = nullptr;
        }
        return ::DefSubclassProc(window, message, wparam, lparam);
    }

    HWND host_ = nullptr;
    HWND event_target_ = nullptr;
    mwtl::DockGroupId group_{};
    mwtl::TabControl tabs_;
    std::vector<std::pair<mwtl::DockPanelId, HWND>> panels_;
};

mwtl::DockLayoutSnapshot DefaultLayout() {
    mwtl::DockLayoutModel model{kDocuments, {100}, mwtl::DockGroupRole::document};
    if (!model.AddDockedGroup(kLeftTools, {200}, mwtl::DockGroupRole::tool,
            kDocuments, {300}, mwtl::DockEdge::left, 0.23) ||
        !model.AddDockedGroup(kBottomTools, {201}, mwtl::DockGroupRole::tool,
            kDocuments, {301}, mwtl::DockEdge::bottom, 0.72) ||
        !model.AddPanel({kMainDocument, L"main.cpp", mwtl::DockPanelRole::document},
                        kDocuments) ||
        !model.AddPanel({kReadmeDocument, L"README.md", mwtl::DockPanelRole::document},
                        kDocuments) ||
        !model.AddPanel({kSolutionExplorer, L"Solution Explorer",
                         mwtl::DockPanelRole::tool}, kLeftTools) ||
        !model.AddPanel({kOutputPanel, L"Output", mwtl::DockPanelRole::tool},
                        kBottomTools))
        throw std::runtime_error("build default docking layout failed");
    return model.GetSnapshot();
}

class DockingIdeWindow final : public mwtl::WindowBase {
public:
    DockingIdeWindow()
        : model_{kDocuments, {100}, mwtl::DockGroupRole::document} {
        const auto defaults = DefaultLayout();
        mwtl::Must(static_cast<bool>(model_.Replace(defaults)), "initialize docking model");
    }

    void BuildUI() override {
        BuildCommands();
        mwtl::ControlHost controls{*this};
        controls.Add(toolbar_);
        controls.Add(status_);
        for (const auto& command : commands_.GetCommands()) {
            if (command.GetId() != kExit)
                mwtl::Must(toolbar_.AddCommand(command), "add docking toolbar command");
        }
        toolbar_.AutoSize();
        BuildMenu();
        mwtl::Must(accelerators_.Create(commands_), "create docking accelerators");
        SetAccelerators(accelerators_.GetHandle());
        const std::array<int, 2> status_parts{620, -1};
        status_.SetParts(status_parts);
        mwtl::SetAccessibleName(status_.GetHwnd(), L"Docking workspace status");

        mwtl::Must(outer_.Create(*this, kOuterSplitter,
            {0.0_dip, 38.0_dip, 1000.0_dip, 600.0_dip},
            {.orientation = mwtl::SplitterOrientation::vertical,
             .constraints = {160.0_dip, 360.0_dip, 6.0_dip},
             .initial_position = 230.0_dip}), "create outer splitter");
        mwtl::Must(left_.Create(outer_.GetHwnd(), GetHwnd(), 1001, kLeftTools,
                                L"Left tool group"), "create left group");
        right_holder_ = CreatePane(outer_.GetHwnd(), 1002, L"Document and output region");
        mwtl::Must(right_holder_ != nullptr && outer_.AttachPanes(left_.GetHwnd(), right_holder_),
                   "attach outer panes");
        mwtl::Must(::SetWindowSubclass(right_holder_, RightProcedure, 1002,
            reinterpret_cast<DWORD_PTR>(this)) != FALSE, "subclass right pane");
        mwtl::Must(inner_.Create(right_holder_, kInnerSplitter,
            {0.0_dip, 0.0_dip, 700.0_dip, 560.0_dip},
            {.orientation = mwtl::SplitterOrientation::horizontal,
             .constraints = {220.0_dip, 110.0_dip, 6.0_dip},
             .initial_position = 410.0_dip}), "create inner splitter");
        mwtl::Must(documents_.Create(inner_.GetHwnd(), GetHwnd(), 1003, kDocuments,
                                     L"Document tab group"), "create document group");
        mwtl::Must(bottom_.Create(inner_.GetHwnd(), GetHwnd(), 1004, kBottomTools,
                                  L"Bottom tool group"), "create bottom group");
        mwtl::Must(inner_.AttachPanes(documents_.GetHwnd(), bottom_.GetHwnd()),
                   "attach inner panes");
        mwtl::Must(floating_group_.Create(GetHwnd(), GetHwnd(), 1005, kFloatingTools,
                                          L"Floating tool group"),
                   "create floating group");
        ::ShowWindow(floating_group_.GetHwnd(), SW_HIDE);

        CreatePanels();
        BindNativeState();
        RestoreLayout();
        Synchronize(L"Ready — use toolbar, menu, shortcuts, or tab selection");
        mwtl::ApplyWindowAppearance(GetHwnd(),
            {mwtl::ColorMode::system, mwtl::Backdrop::mica});
        if (g_self_test && !::PostMessageW(GetHwnd(), kRunSelfTest, 0, 0))
            throw std::runtime_error("post docking self-test failed");
    }

    mwtl::EventResult OnCommand(const mwtl::CommandEvent& event) override {
        return commands_.Dispatch(event);
    }

    mwtl::EventResult OnNotify(const mwtl::NotifyEvent& event) override {
        for (GroupView* view : Views()) {
            if (!event.Is(view->Tabs(), TCN_SELCHANGE)) continue;
            const auto selected = view->Tabs().GetSelectedTabId();
            if (selected) {
                mwtl::DockMutation activate;
                activate.kind = mwtl::DockMutationKind::activate;
                activate.panel = {selected->value};
                ApplyMutation(activate, L"Active panel changed");
            }
            return mwtl::EventResult::Handled();
        }
        return mwtl::EventResult::Propagate();
    }

    mwtl::EventResult OnResize(const mwtl::ResizeEvent&) override {
        LayoutChrome();
        return mwtl::EventResult::Handled();
    }

    mwtl::EventResult OnKeyDown(const mwtl::KeyEvent& event) override {
        if (!keyboard_.IsActive()) return mwtl::EventResult::Propagate();
        if (event.virtual_key == VK_ESCAPE) {
            keyboard_.Cancel();
            preview_.Hide();
            SetStatus(L"Keyboard docking cancelled");
            return mwtl::EventResult::Handled();
        }
        if (event.virtual_key == VK_RETURN) {
            const auto accepted = keyboard_.Accept();
            preview_.Hide();
            if (accepted) ApplyKeyboardTarget(accepted->target);
            return mwtl::EventResult::Handled();
        }
        std::optional<mwtl::DockKeyboardMove> move;
        if (event.virtual_key == VK_LEFT) move = mwtl::DockKeyboardMove::left;
        else if (event.virtual_key == VK_RIGHT) move = mwtl::DockKeyboardMove::right;
        else if (event.virtual_key == VK_UP) move = mwtl::DockKeyboardMove::up;
        else if (event.virtual_key == VK_DOWN) move = mwtl::DockKeyboardMove::down;
        else if (event.virtual_key == VK_TAB)
            move = (::GetKeyState(VK_SHIFT) < 0) ? mwtl::DockKeyboardMove::previous
                                                : mwtl::DockKeyboardMove::next;
        if (!move) return mwtl::EventResult::Propagate();
        keyboard_.Move(*move);
        ShowKeyboardSelection();
        return mwtl::EventResult::Handled();
    }

    mwtl::EventResult OnMessage(const mwtl::WindowMessage& event) override {
        if (event.id != kRunSelfTest) return mwtl::EventResult::Propagate();
        try {
            RunSelfTest();
            ::PostQuitMessage(0);
        } catch (...) {
            ::PostQuitMessage(self_test_step_);
        }
        return mwtl::EventResult::Handled();
    }

    mwtl::EventResult OnClose() override {
        SaveLayout();
        preview_.Destroy();
        floating_.Destroy();
        native_.Detach();
        if (right_holder_ && ::IsWindow(right_holder_))
            ::RemoveWindowSubclass(right_holder_, RightProcedure, 1002);
        return mwtl::EventResult::Propagate();
    }

private:
    static LRESULT CALLBACK RightProcedure(HWND window, UINT message, WPARAM wparam,
        LPARAM lparam, UINT_PTR subclass_id, DWORD_PTR reference) noexcept {
        auto* self = reinterpret_cast<DockingIdeWindow*>(reference);
        if (message == WM_SIZE && self) self->LayoutInner();
        if (message == WM_NOTIFY && self)
            return ::SendMessageW(self->GetHwnd(), message, wparam, lparam);
        if (message == WM_NCDESTROY)
            ::RemoveWindowSubclass(window, RightProcedure, subclass_id);
        return ::DefSubclassProc(window, message, wparam, lparam);
    }

    std::array<GroupView*, 4> Views() noexcept {
        return {&documents_, &left_, &bottom_, &floating_group_};
    }

    void BuildCommands() {
        commands_
            .Add(mwtl::Command(kFloatOutput, L"Float Output", [this] { FloatOutput(); })
                .SetShortcut({FVIRTKEY | FCONTROL | FSHIFT, 'F'}))
            .Add(mwtl::Command(kDockOutput, L"Dock Output", [this] { RedockOutput(); })
                .SetShortcut({FVIRTKEY | FCONTROL | FSHIFT, 'D'}))
            .Add(mwtl::Command(kAutoHideExplorer, L"Auto-hide Explorer",
                [this] { AutoHideExplorer(); }))
            .Add(mwtl::Command(kPinExplorer, L"Pin Explorer",
                [this] { PinExplorer(); }))
            .Add(mwtl::Command(kKeyboardDock, L"Keyboard Dock Output",
                [this] { BeginKeyboardDock(); }).SetShortcut({FVIRTKEY | FCONTROL, 'K'}))
            .Add(mwtl::Command(kSaveLayout, L"Save Layout",
                [this] { SaveLayout(); }).SetShortcut({FVIRTKEY | FCONTROL, 'S'}))
            .Add(mwtl::Command(kResetLayout, L"Reset Layout",
                [this] { ApplySnapshot(DefaultLayout(), L"Default layout restored"); }))
            .Add(mwtl::Command(kExit, L"Exit", [this] { Close(); }));
    }

    void BuildMenu() {
        mwtl::Menu bar, popup;
        mwtl::Must(bar.Create() && popup.CreatePopup(), "create docking menu");
        for (mwtl::ControlId id : {kFloatOutput, kDockOutput, kAutoHideExplorer,
                kPinExplorer, kKeyboardDock, kSaveLayout, kResetLayout})
            mwtl::Must(popup.AppendCommand(*commands_.Find(id)), "append docking command");
        popup.AppendSeparator();
        mwtl::Must(popup.AppendCommand(*commands_.Find(kExit)), "append exit command");
        mwtl::Must(bar.AppendSubmenu(std::move(popup), L"Workspace") &&
                   bar.AttachToWindow(GetHwnd()), "attach docking menu");
    }

    void CreatePanels() {
        main_editor_ = ::CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT",
            L"// main.cpp\r\nint main() { return 0; }\r\n",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_MULTILINE | WS_VSCROLL,
            0, 0, 10, 10, documents_.GetHwnd(), reinterpret_cast<HMENU>(1101),
            ::GetModuleHandleW(nullptr), nullptr);
        readme_editor_ = ::CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT",
            L"# Agent-friendly native IDE workspace\r\n\r\nUse Ctrl+K to dock by keyboard.",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_MULTILINE | WS_VSCROLL,
            0, 0, 10, 10, documents_.GetHwnd(), reinterpret_cast<HMENU>(1102),
            ::GetModuleHandleW(nullptr), nullptr);
        explorer_ = ::CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", nullptr,
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | LBS_NOTIFY,
            0, 0, 10, 10, left_.GetHwnd(), reinterpret_cast<HMENU>(1103),
            ::GetModuleHandleW(nullptr), nullptr);
        output_ = ::CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT",
            L"Build started...\r\nAll x64 checks passed.\r\n",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_MULTILINE | ES_READONLY | WS_VSCROLL,
            0, 0, 10, 10, bottom_.GetHwnd(), reinterpret_cast<HMENU>(1104),
            ::GetModuleHandleW(nullptr), nullptr);
        mwtl::Must(main_editor_ && readme_editor_ && explorer_ && output_,
                   "create docking panels");
        for (const wchar_t* item : {L"include", L"src", L"tests", L"examples"})
            ::SendMessageW(explorer_, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(item));
        for (auto [id, window] : PanelBindings()) {
            documents_.RegisterPanel(id, window);
            left_.RegisterPanel(id, window);
            bottom_.RegisterPanel(id, window);
            floating_group_.RegisterPanel(id, window);
            mwtl::SetAccessibleName(window, model_.FindPanel(id)->title.c_str());
        }
    }

    std::array<std::pair<mwtl::DockPanelId, HWND>, 4> PanelBindings() noexcept {
        return {{{kMainDocument, main_editor_}, {kReadmeDocument, readme_editor_},
                 {kSolutionExplorer, explorer_}, {kOutputPanel, output_}}};
    }

    void BindNativeState() {
        mwtl::Must(static_cast<bool>(native_.Attach(GetHwnd())), "attach docking native adapter");
        for (auto [id, host] : std::array{
                std::pair{kDocuments, documents_.GetHwnd()},
                std::pair{kLeftTools, left_.GetHwnd()},
                std::pair{kBottomTools, bottom_.GetHwnd()},
                std::pair{kFloatingTools, floating_group_.GetHwnd()}})
            mwtl::Must(static_cast<bool>(native_.BindGroup(id, host)), "bind docking group");
        for (auto [id, window] : PanelBindings())
            mwtl::Must(static_cast<bool>(native_.BindPanel(id, window)), "bind docking panel");
    }

    bool EnsureFloating(const mwtl::DockFloatingPlacement& placement =
        {720, 120, 520, 340, L"primary"}) {
        if (!floating_.GetHwnd()) {
            mwtl::DockFloatingWindowOptions options;
            options.title = L"Output — Floating";
            options.placement = placement;
            options.target_dpi = GetDpiContext().GetDpi();
            options.on_close = [this] {
                return RedockOutput() ? mwtl::DockFloatingCloseAction::hide
                                      : mwtl::DockFloatingCloseAction::cancel;
            };
            if (!floating_.Create(GetHwnd(), std::move(options)) ||
                !floating_.AttachContent(floating_group_.GetHwnd())) return false;
        }
        floating_.Show(false);
        return true;
    }

    bool ApplyMutation(const mwtl::DockMutation& mutation, std::wstring_view message) {
        const auto transaction = model_.Propose(mutation);
        if (!transaction) return false;
        auto adoption = native_.Prepare(transaction->proposed);
        if (!adoption || !native_.Adopt(*adoption)) return false;
        if (!model_.Commit(*transaction)) {
            native_.Rollback(*adoption);
            return false;
        }
        if (!native_.Commit(*adoption)) return false;
        Synchronize(message);
        return true;
    }

    bool ApplySnapshot(mwtl::DockLayoutSnapshot snapshot, std::wstring_view message) {
        const auto floating = std::find_if(snapshot.floating_hosts.begin(),
            snapshot.floating_hosts.end(), [](const auto& host) { return host.id.value == 400; });
        if (floating != snapshot.floating_hosts.end() && !EnsureFloating(floating->placement))
            return false;
        auto adoption = native_.Prepare(snapshot);
        if (!adoption || !native_.Adopt(*adoption)) return false;
        if (!model_.Replace(std::move(snapshot))) {
            native_.Rollback(*adoption);
            return false;
        }
        if (!native_.Commit(*adoption)) return false;
        if (model_.GetSnapshot().floating_hosts.empty() && floating_.GetHwnd()) {
            floating_.DetachContent();
            floating_.Hide();
        }
        Synchronize(message);
        return true;
    }

    bool FloatOutput() {
        if (model_.FindPanelGroup(kOutputPanel) == kFloatingTools) return true;
        const mwtl::DockFloatingPlacement placement{720, 120, 520, 340, L"primary"};
        if (!EnsureFloating(placement)) return false;
        mwtl::DockMutation mutation;
        mutation.kind = mwtl::DockMutationKind::float_panel;
        mutation.panel = kOutputPanel;
        mutation.new_group = kFloatingTools;
        mutation.new_group_node = {202};
        mutation.new_floating_host = {400};
        mutation.floating_placement = placement;
        return ApplyMutation(mutation, L"Output floated — close its tool window to redock");
    }

    bool RedockOutput() {
        if (model_.FindPanelGroup(kOutputPanel) == kBottomTools) return true;
        const bool result = ApplyMutation(
            mwtl::MakePinDockMutation(kOutputPanel, kBottomTools),
            L"Output redocked below documents");
        if (result && floating_.GetHwnd()) {
            floating_.DetachContent();
            floating_.Hide();
        }
        return result;
    }

    bool AutoHideExplorer() {
        if (model_.IsAutoHidden(kSolutionExplorer)) return true;
        mwtl::DockMutation mutation;
        mutation.kind = mwtl::DockMutationKind::auto_hide;
        mutation.panel = kSolutionExplorer;
        mutation.edge = mwtl::DockEdge::left;
        return ApplyMutation(mutation, L"Solution Explorer auto-hidden; Pin restores it");
    }

    bool PinExplorer() {
        if (model_.FindPanelGroup(kSolutionExplorer) == kLeftTools) return true;
        return ApplyMutation(mwtl::MakePinDockMutation(kSolutionExplorer, kLeftTools),
                             L"Solution Explorer pinned");
    }

    std::vector<mwtl::DockDropTarget> KeyboardTargets() const {
        std::vector<mwtl::DockDropTarget> result;
        const auto dpi = GetDpiContext();
        const auto add_group = [&](std::uint64_t id, HWND window,
                                   mwtl::DockGroupId group) {
            RECT pixels{};
            ::GetWindowRect(window, &pixels);
            result.push_back({id, mwtl::DockTargetKind::tab_group,
                {dpi.FromPixels(pixels.left).value, dpi.FromPixels(pixels.top).value,
                 dpi.FromPixels(pixels.right - pixels.left).value,
                 dpi.FromPixels(pixels.bottom - pixels.top).value}, group});
        };
        add_group(1, left_.GetHwnd(), kLeftTools);
        add_group(2, bottom_.GetHwnd(), kBottomTools);
        RECT main{};
        ::GetWindowRect(GetHwnd(), &main);
        result.push_back({3, mwtl::DockTargetKind::auto_hide,
            {dpi.FromPixels(main.left).value,
             dpi.FromPixels(main.bottom - 42).value,
             dpi.FromPixels(main.right - main.left).value, 42.0}, {},
             mwtl::DockEdge::bottom});
        result.push_back({4, mwtl::DockTargetKind::floating,
            {dpi.FromPixels(main.right - 180).value,
             dpi.FromPixels(main.top + 60).value, 140.0, 100.0}});
        return result;
    }

    void BeginKeyboardDock() {
        keyboard_.Reset();
        keyboard_targets_ = KeyboardTargets();
        if (!keyboard_.Begin(kOutputPanel, keyboard_targets_, 2)) {
            SetStatus(L"No keyboard docking targets are available");
            return;
        }
        if (!preview_.GetHwnd()) preview_.Create(GetHwnd());
        ::SetFocus(GetHwnd());
        ShowKeyboardSelection();
    }

    void ShowKeyboardSelection() {
        const auto selection = keyboard_.GetSelection();
        if (!selection) return;
        preview_.Show(selection->target.bounds, GetDpiContext().GetDpi());
        SetStatus(selection->announcement + L" — arrows/Tab move, Enter accepts, Escape cancels");
    }

    void ApplyKeyboardTarget(const mwtl::DockDropTarget& target) {
        if (target.kind == mwtl::DockTargetKind::tab_group)
            ApplyMutation(mwtl::MakePinDockMutation(kOutputPanel, target.group),
                          L"Output docked by keyboard");
        else if (target.kind == mwtl::DockTargetKind::auto_hide) {
            mwtl::DockMutation mutation;
            mutation.kind = mwtl::DockMutationKind::auto_hide;
            mutation.panel = kOutputPanel;
            mutation.edge = target.edge;
            ApplyMutation(mutation, L"Output auto-hidden by keyboard");
        } else if (target.kind == mwtl::DockTargetKind::floating) {
            FloatOutput();
        }
    }

    void Synchronize(std::wstring_view message) {
        mwtl::Must(static_cast<bool>(native_.Synchronize(model_.GetSnapshot())),
                   "synchronize docking HWNDs");
        for (GroupView* view : Views()) view->Synchronize(model_.GetSnapshot());
        LayoutChrome();
        SetStatus(message);
    }

    void SetStatus(std::wstring_view message) {
        status_.SetPartText(0, message);
        const auto active = model_.GetSnapshot().active_panel;
        status_.SetPartText(1, active && model_.FindPanel(*active)
            ? model_.FindPanel(*active)->title : L"No active panel");
    }

    void LayoutChrome() noexcept {
        RECT client{};
        if (!::GetClientRect(GetHwnd(), &client)) return;
        constexpr int toolbar_height = 34;
        constexpr int status_height = 24;
        ::SetWindowPos(toolbar_.GetHwnd(), nullptr, 0, 0, client.right, toolbar_height,
                       SWP_NOZORDER | SWP_NOACTIVATE);
        ::SetWindowPos(status_.GetHwnd(), nullptr, 0, client.bottom - status_height,
                       client.right, status_height, SWP_NOZORDER | SWP_NOACTIVATE);
        const auto dpi = GetDpiContext();
        const int client_width = static_cast<int>(client.right);
        const int workspace_height = (std::max)(0,
            static_cast<int>(client.bottom) - toolbar_height - status_height);
        outer_.SetBounds(mwtl::RectDip{0.0_dip, dpi.FromPixels(toolbar_height),
            dpi.FromPixels(client_width), dpi.FromPixels(workspace_height)});
        LayoutInner();
    }

    void LayoutInner() noexcept {
        if (!right_holder_ || !::IsWindow(right_holder_)) return;
        RECT client{};
        ::GetClientRect(right_holder_, &client);
        const auto dpi = mwtl::DpiContext::FromWindow(right_holder_);
        inner_.SetBounds({0.0_dip, 0.0_dip, dpi.FromPixels(client.right),
                          dpi.FromPixels(client.bottom)});
    }

    std::filesystem::path SessionPath() const {
        if (g_self_test) return std::filesystem::temp_directory_path() /
            (L"mwtl-docking-workspace-" + std::to_wstring(::GetCurrentProcessId()) +
             L".state");
        std::array<wchar_t, 32768> local{};
        const DWORD length = ::GetEnvironmentVariableW(
            L"LOCALAPPDATA", local.data(), static_cast<DWORD>(local.size()));
        if (length == 0 || length >= local.size()) return {};
        return std::filesystem::path{local.data()} / L"mwtl" / L"docking-workspace.state";
    }

    bool SaveLayout() {
        const auto path = SessionPath();
        if (path.empty()) return false;
        std::error_code error;
        std::filesystem::create_directories(path.parent_path(), error);
        const bool saved = mwtl::SaveDockingSessionAtomic(path, model_.GetSnapshot()) ==
                           mwtl::DockingSessionStatus::success;
        SetStatus(saved ? L"Layout saved" : L"Layout save failed");
        return saved;
    }

    void RestoreLayout() {
        const auto path = SessionPath();
        if (path.empty()) return;
        const auto loaded = mwtl::LoadDockingSession(path);
        if (!loaded) return;
        const auto& snapshot = *loaded.snapshot;
        const bool known = snapshot.panels.size() == 4 &&
            std::all_of(snapshot.panels.begin(), snapshot.panels.end(), [](const auto& panel) {
                return panel.id.value >= 1 && panel.id.value <= 4;
            });
        if (known) ApplySnapshot(snapshot, L"Saved layout restored");
    }

    void RunSelfTest() {
        self_test_step_ = 2;
        if (model_.GetSnapshot().panels.size() != 4 ||
            model_.FindPanelGroup(kOutputPanel) != kBottomTools ||
            ::GetParent(output_) != bottom_.GetHwnd())
            throw std::runtime_error("default docking composition failed");
        self_test_step_ = 3;
        if (!FloatOutput() || model_.FindPanelGroup(kOutputPanel) != kFloatingTools ||
            ::GetParent(output_) != floating_group_.GetHwnd() || !floating_.IsVisible())
            throw std::runtime_error("float workflow failed");
        self_test_step_ = 4;
        if (!RedockOutput() || model_.FindPanelGroup(kOutputPanel) != kBottomTools ||
            ::GetParent(output_) != bottom_.GetHwnd())
            throw std::runtime_error("redock workflow failed");
        self_test_step_ = 5;
        if (!AutoHideExplorer() || !model_.IsAutoHidden(kSolutionExplorer) ||
            ::IsWindowVisible(explorer_))
            throw std::runtime_error("auto-hide workflow failed");
        if (!PinExplorer() || model_.FindPanelGroup(kSolutionExplorer) != kLeftTools)
            throw std::runtime_error("pin workflow failed");
        self_test_step_ = 6;
        BeginKeyboardDock();
        if (!keyboard_.IsActive() || !keyboard_.Move(mwtl::DockKeyboardMove::left))
            throw std::runtime_error("keyboard docking navigation failed");
        const auto accepted = keyboard_.Accept();
        preview_.Hide();
        if (!accepted) throw std::runtime_error("keyboard docking accept failed");
        ApplyKeyboardTarget(accepted->target);
        self_test_step_ = 7;
        if (!SaveLayout()) throw std::runtime_error("layout save failed");
        const auto loaded = mwtl::LoadDockingSession(SessionPath());
        if (!loaded || loaded.snapshot->panels.size() != 4)
            throw std::runtime_error("layout restore file failed");
        self_test_step_ = 8;
        const std::array monitors{
            mwtl::DockMonitorWorkArea{L"primary", 0, 0, 1920, 1040, 96, true}};
        if (!mwtl::RecoverDockFloatingPlacement(
                {5000, 5000, 600, 400, L"removed"}, monitors))
            throw std::runtime_error("monitor recovery failed");
        std::error_code ignored;
        std::filesystem::remove(SessionPath(), ignored);
        floating_.Destroy();
        preview_.Destroy();
        native_.Detach();
    }

    mwtl::DockLayoutModel model_;
    mwtl::DockNativeWorkspaceAdapter native_;
    mwtl::DockFloatingWindow floating_;
    mwtl::DockPreviewWindow preview_;
    mwtl::DockKeyboardSession keyboard_;
    std::vector<mwtl::DockDropTarget> keyboard_targets_;
    mwtl::CommandSet commands_;
    mwtl::AcceleratorTable accelerators_;
    mwtl::Toolbar toolbar_;
    mwtl::StatusBar status_;
    mwtl::Splitter outer_;
    mwtl::Splitter inner_;
    GroupView documents_, left_, bottom_, floating_group_;
    HWND right_holder_ = nullptr;
    HWND main_editor_ = nullptr;
    HWND readme_editor_ = nullptr;
    HWND explorer_ = nullptr;
    HWND output_ = nullptr;
    int self_test_step_ = 1;
};

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    g_self_test = wcsstr(::GetCommandLineW(), L"--self-test") != nullptr;
    return mwtl::RunApplication<DockingIdeWindow>(instance, show,
        {.title = L"mwtl Docking IDE Workspace",
         .initial_bounds = {{40.0_dip, 40.0_dip}, {1180.0_dip, 760.0_dip}},
         .use_default_bounds = false});
}
