#include "application.h"
#include <mwfl/mwfl.h>

#include <commctrl.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <stdexcept>
#include <string_view>
#include <vector>

using mwfl::operator""_dip;

namespace {

constexpr mwfl::ControlId kFloatOutput{900};
constexpr mwfl::ControlId kDockOutput{901};
constexpr mwfl::ControlId kAutoHideExplorer{902};
constexpr mwfl::ControlId kPinExplorer{903};
constexpr mwfl::ControlId kKeyboardDock{904};
constexpr mwfl::ControlId kSaveLayout{905};
constexpr mwfl::ControlId kResetLayout{906};
constexpr mwfl::ControlId kExit{907};
constexpr mwfl::ControlId kReorderDocuments{908};
constexpr mwfl::ControlId kCloseReadme{909};
constexpr mwfl::ControlId kToolbar{910};
constexpr mwfl::ControlId kStatus{911};
constexpr mwfl::ControlId kOuterSplitter{912};
constexpr mwfl::ControlId kInnerSplitter{913};
constexpr UINT kRunSelfTest = WM_APP + 0x180;
constexpr UINT kDockMouseBegin = WM_APP + 0x181;
constexpr UINT kDockMouseMove = WM_APP + 0x182;
constexpr UINT kDockMouseEnd = WM_APP + 0x183;
constexpr UINT kDockMouseCancel = WM_APP + 0x184;

constexpr mwfl::DockPanelId kMainDocument{1};
constexpr mwfl::DockPanelId kReadmeDocument{2};
constexpr mwfl::DockPanelId kSolutionExplorer{3};
constexpr mwfl::DockPanelId kOutputPanel{4};
constexpr mwfl::DockGroupId kDocuments{10};
constexpr mwfl::DockGroupId kLeftTools{20};
constexpr mwfl::DockGroupId kBottomTools{30};
constexpr mwfl::DockGroupId kFloatingTools{31};

bool g_self_test = false;
bool g_showcase = false;

struct DockMouseEvent {
    mwfl::DockPanelId panel{};
    POINT screen{};
};

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
                mwfl::DockGroupId group, const wchar_t* accessible_name) {
        event_target_ = event_target;
        group_ = group;
        host_ = CreatePane(parent, id, accessible_name);
        if (!host_ || !tabs_.Create(host_, {id + 100},
                {0.0_dip, 0.0_dip, 100.0_dip, 28.0_dip}) ||
            !mwfl::SetAccessibleName(host_, accessible_name) ||
            !mwfl::SetAccessibleName(tabs_.GetHwnd(), accessible_name) ||
            !::SetWindowSubclass(host_, Procedure,
                static_cast<UINT_PTR>(id), reinterpret_cast<DWORD_PTR>(this)) ||
            !::SetWindowSubclass(tabs_.GetHwnd(), TabProcedure,
                static_cast<UINT_PTR>(id + 100), reinterpret_cast<DWORD_PTR>(this)))
            return false;
        return true;
    }

    void RegisterPanel(mwfl::DockPanelId id, HWND window) {
        panels_.push_back({id, window});
    }

    void Synchronize(const mwfl::DockLayoutSnapshot& snapshot) {
        for (const auto& panel : panels_)
            tabs_.RemoveTab(mwfl::TabId{panel.first.value});
        const auto found = std::find_if(snapshot.groups.begin(), snapshot.groups.end(),
            [&](const mwfl::DockGroup& group) { return group.id == group_; });
        if (found != snapshot.groups.end()) {
            for (mwfl::DockPanelId id : found->panels) {
                const auto panel = std::find_if(snapshot.panels.begin(), snapshot.panels.end(),
                    [&](const mwfl::DockPanel& value) { return value.id == id; });
                if (panel != snapshot.panels.end())
                    tabs_.AddTab(mwfl::TabId{id.value}, panel->title);
            }
            if (found->active) tabs_.SetSelection(mwfl::TabId{found->active->value});
        }
        Arrange();
    }

    void Arrange() noexcept {
        if (!host_) return;
        RECT client{};
        ::GetClientRect(host_, &client);
        const int tab_height = 27;
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
    mwfl::TabControl& Tabs() noexcept { return tabs_; }
    mwfl::DockGroupId GetGroup() const noexcept { return group_; }

private:
    void SendMouseEvent(UINT message, POINT screen) noexcept {
        if (!event_target_ || !drag_panel_) return;
        DockMouseEvent event{drag_panel_, screen};
        ::SendMessageW(event_target_, message, 0,
                       reinterpret_cast<LPARAM>(&event));
    }

    static LRESULT CALLBACK TabProcedure(HWND window, UINT message, WPARAM wparam,
                                         LPARAM lparam, UINT_PTR subclass_id,
                                         DWORD_PTR reference) noexcept {
        auto* self = reinterpret_cast<GroupView*>(reference);
        if (!self) return ::DefSubclassProc(window, message, wparam, lparam);
        if (message == WM_LBUTTONDOWN) {
            const LRESULT result = ::DefSubclassProc(window, message, wparam, lparam);
            const auto selected = self->tabs_.GetSelectedTabId();
            self->drag_panel_ = selected ? mwfl::DockPanelId{selected->value}
                                         : mwfl::DockPanelId{};
            ::GetCursorPos(&self->drag_start_);
            self->dragging_ = false;
            if (self->drag_panel_) ::SetCapture(window);
            return result;
        }
        if (message == WM_MOUSEMOVE && self->drag_panel_ &&
            ::GetCapture() == window) {
            POINT screen{};
            ::GetCursorPos(&screen);
            const int dx = (std::max)(::GetSystemMetrics(SM_CXDRAG), 1);
            const int dy = (std::max)(::GetSystemMetrics(SM_CYDRAG), 1);
            if (!self->dragging_ &&
                (std::abs(screen.x - self->drag_start_.x) >= dx ||
                 std::abs(screen.y - self->drag_start_.y) >= dy)) {
                self->dragging_ = true;
                self->SendMouseEvent(kDockMouseBegin, screen);
            }
            if (self->dragging_) self->SendMouseEvent(kDockMouseMove, screen);
            return 0;
        }
        if (message == WM_KEYDOWN && wparam == VK_ESCAPE && self->drag_panel_) {
            const bool notify = self->dragging_;
            self->dragging_ = false;
            self->drag_panel_ = {};
            if (::GetCapture() == window) ::ReleaseCapture();
            if (notify && self->event_target_)
                ::SendMessageW(self->event_target_, kDockMouseCancel, 0, 0);
            return 0;
        }
        if (message == WM_LBUTTONUP && self->drag_panel_) {
            POINT screen{};
            ::GetCursorPos(&screen);
            const bool dragging = self->dragging_;
            self->dragging_ = false;
            if (dragging) self->SendMouseEvent(kDockMouseEnd, screen);
            self->drag_panel_ = {};
            if (::GetCapture() == window) ::ReleaseCapture();
            if (dragging) return 0;
        }
        if ((message == WM_CAPTURECHANGED || message == WM_CANCELMODE) &&
            self->drag_panel_) {
            const bool notify = self->dragging_;
            self->dragging_ = false;
            self->drag_panel_ = {};
            if (notify && self->event_target_)
                ::SendMessageW(self->event_target_, kDockMouseCancel, 0, 0);
        }
        if (message == WM_NCDESTROY) {
            if (self->dragging_ && self->event_target_)
                ::SendMessageW(self->event_target_, kDockMouseCancel, 0, 0);
            self->dragging_ = false;
            self->drag_panel_ = {};
            ::RemoveWindowSubclass(window, TabProcedure, subclass_id);
        }
        return ::DefSubclassProc(window, message, wparam, lparam);
    }

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
    mwfl::DockGroupId group_{};
    mwfl::TabControl tabs_;
    std::vector<std::pair<mwfl::DockPanelId, HWND>> panels_;
    mwfl::DockPanelId drag_panel_{};
    POINT drag_start_{};
    bool dragging_ = false;
};

mwfl::DockLayoutSnapshot DefaultLayout() {
    mwfl::DockLayoutModel model{kDocuments, {100}, mwfl::DockGroupRole::document};
    if (!model.AddDockedGroup(kLeftTools, {200}, mwfl::DockGroupRole::tool,
            kDocuments, {300}, mwfl::DockEdge::left, 0.23) ||
        !model.AddDockedGroup(kBottomTools, {201}, mwfl::DockGroupRole::tool,
            kDocuments, {301}, mwfl::DockEdge::bottom, 0.72) ||
        !model.AddPanel({kMainDocument, L"main.cpp", mwfl::DockPanelRole::document},
                        kDocuments) ||
        !model.AddPanel({kReadmeDocument, L"README.md", mwfl::DockPanelRole::document},
                        kDocuments) ||
        !model.AddPanel({kSolutionExplorer, L"Solution Explorer",
                         mwfl::DockPanelRole::tool}, kLeftTools) ||
        !model.AddPanel({kOutputPanel, L"Output", mwfl::DockPanelRole::tool},
                        kBottomTools))
        throw std::runtime_error("build default docking layout failed");
    return model.GetSnapshot();
}

class DockingIdeWindow final : public mwfl::WindowBase {
public:
    ~DockingIdeWindow() override {
        if (code_font_ != nullptr) ::DeleteObject(code_font_);
    }

    DockingIdeWindow()
        : model_{kDocuments, {100}, mwfl::DockGroupRole::document} {
        const auto defaults = DefaultLayout();
        mwfl::Must(static_cast<bool>(model_.Replace(defaults)), "initialize docking model");
    }

    void BuildUI() override {
        BuildCommands();
        mwfl::ControlHost controls{*this};
        controls.Add(toolbar_);
        controls.Add(status_);
        for (const mwfl::ControlId id :
             {kFloatOutput, kAutoHideExplorer, kKeyboardDock, kSaveLayout})
            mwfl::Must(toolbar_.AddCommand(*commands_.Find(id)),
                       "add primary docking toolbar command");
        toolbar_.AutoSize();
        BuildMenu();
        mwfl::Must(accelerators_.Create(commands_), "create docking accelerators");
        SetAccelerators(accelerators_.GetHandle());
        const std::array<int, 2> status_parts{620, -1};
        status_.SetParts(status_parts);
        mwfl::SetAccessibleName(status_.GetHwnd(), L"Docking workspace status");

        mwfl::Must(outer_.Create(*this, kOuterSplitter,
            {0.0_dip, 38.0_dip, 1000.0_dip, 600.0_dip},
            {.orientation = mwfl::SplitterOrientation::vertical,
             .constraints = {160.0_dip, 360.0_dip, 6.0_dip},
             .initial_position = 230.0_dip}), "create outer splitter");
        mwfl::Must(left_.Create(outer_.GetHwnd(), GetHwnd(), 1001, kLeftTools,
                                L"Left tool group"), "create left group");
        right_holder_ = CreatePane(outer_.GetHwnd(), 1002, L"Document and output region");
        mwfl::Must(right_holder_ != nullptr && outer_.AttachPanes(left_.GetHwnd(), right_holder_),
                   "attach outer panes");
        mwfl::Must(::SetWindowSubclass(right_holder_, RightProcedure, 1002,
            reinterpret_cast<DWORD_PTR>(this)) != FALSE, "subclass right pane");
        mwfl::Must(inner_.Create(right_holder_, kInnerSplitter,
            {0.0_dip, 0.0_dip, 700.0_dip, 560.0_dip},
            {.orientation = mwfl::SplitterOrientation::horizontal,
             .constraints = {220.0_dip, 110.0_dip, 6.0_dip},
             .initial_position = 410.0_dip}), "create inner splitter");
        mwfl::Must(documents_.Create(inner_.GetHwnd(), GetHwnd(), 1003, kDocuments,
                                     L"Document tab group"), "create document group");
        mwfl::Must(bottom_.Create(inner_.GetHwnd(), GetHwnd(), 1004, kBottomTools,
                                  L"Bottom tool group"), "create bottom group");
        mwfl::Must(inner_.AttachPanes(documents_.GetHwnd(), bottom_.GetHwnd()),
                   "attach inner panes");
        mwfl::Must(floating_group_.Create(GetHwnd(), GetHwnd(), 1005, kFloatingTools,
                                          L"Floating tool group"),
                   "create floating group");
        ::ShowWindow(floating_group_.GetHwnd(), SW_HIDE);

        CreatePanels();
        BindNativeState();
        if (!g_showcase) RestoreLayout();
        Synchronize(L"Ready — use toolbar, menu, shortcuts, or tab selection");
        SetAppearance({mwfl::ColorMode::light, mwfl::Backdrop::mica});
        if (g_self_test && !::PostMessageW(GetHwnd(), kRunSelfTest, 0, 0))
            throw std::runtime_error("post docking self-test failed");
    }

    mwfl::EventResult OnCommand(const mwfl::CommandEvent& event) override {
        return commands_.Dispatch(event);
    }

    mwfl::EventResult OnNotify(const mwfl::NotifyEvent& event) override {
        for (GroupView* view : Views()) {
            if (!event.Is(view->Tabs(), TCN_SELCHANGE)) continue;
            const auto selected = view->Tabs().GetSelectedTabId();
            if (selected) {
                mwfl::DockMutation activate;
                activate.kind = mwfl::DockMutationKind::activate;
                activate.panel = {selected->value};
                ApplyMutation(activate, L"Active panel changed");
            }
            return mwfl::EventResult::Handled();
        }
        return mwfl::EventResult::Propagate();
    }

    mwfl::EventResult OnResize(const mwfl::ResizeEvent&) override {
        LayoutChrome();
        return mwfl::EventResult::Handled();
    }

    mwfl::EventResult OnDpiChanged(const mwfl::DpiChangedEvent& event) override {
        ApplyPanelFont(event.dpi_x);
        return mwfl::EventResult::Propagate();
    }

    mwfl::EventResult OnKeyDown(const mwfl::KeyEvent& event) override {
        if (!keyboard_.IsActive()) return mwfl::EventResult::Propagate();
        if (event.virtual_key == VK_ESCAPE) {
            keyboard_.Cancel();
            preview_.Hide();
            SetStatus(L"Keyboard docking cancelled");
            return mwfl::EventResult::Handled();
        }
        if (event.virtual_key == VK_RETURN) {
            const auto accepted = keyboard_.Accept();
            preview_.Hide();
            if (accepted) ApplyKeyboardTarget(accepted->target);
            return mwfl::EventResult::Handled();
        }
        std::optional<mwfl::DockKeyboardMove> move;
        if (event.virtual_key == VK_LEFT) move = mwfl::DockKeyboardMove::left;
        else if (event.virtual_key == VK_RIGHT) move = mwfl::DockKeyboardMove::right;
        else if (event.virtual_key == VK_UP) move = mwfl::DockKeyboardMove::up;
        else if (event.virtual_key == VK_DOWN) move = mwfl::DockKeyboardMove::down;
        else if (event.virtual_key == VK_TAB)
            move = (::GetKeyState(VK_SHIFT) < 0) ? mwfl::DockKeyboardMove::previous
                                                : mwfl::DockKeyboardMove::next;
        if (!move) return mwfl::EventResult::Propagate();
        keyboard_.Move(*move);
        ShowKeyboardSelection();
        return mwfl::EventResult::Handled();
    }

    mwfl::EventResult OnMessage(const mwfl::WindowMessage& event) override {
        if (event.id == kDockMouseCancel) {
            CancelMouseDock(mwfl::DockDragCancelReason::capture_lost);
            return mwfl::EventResult::Handled();
        }
        if (event.id == kDockMouseBegin || event.id == kDockMouseMove ||
            event.id == kDockMouseEnd) {
            const auto* mouse = reinterpret_cast<const DockMouseEvent*>(event.lparam);
            if (!mouse) return mwfl::EventResult::Handled();
            if (event.id == kDockMouseBegin) BeginMouseDock(mouse->panel, mouse->screen);
            else if (event.id == kDockMouseMove) UpdateMouseDock(mouse->screen);
            else EndMouseDock(mouse->screen);
            return mwfl::EventResult::Handled();
        }
        if (event.id != kRunSelfTest) return mwfl::EventResult::Propagate();
        try {
            RunSelfTest();
            ::PostQuitMessage(0);
        } catch (...) {
            ::PostQuitMessage(self_test_step_);
        }
        return mwfl::EventResult::Handled();
    }

    mwfl::EventResult OnClose() override {
        SaveLayout();
        preview_.Destroy();
        floating_.Destroy();
        native_.Detach();
        if (right_holder_ && ::IsWindow(right_holder_))
            ::RemoveWindowSubclass(right_holder_, RightProcedure, 1002);
        return mwfl::EventResult::Propagate();
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
            .Add(mwfl::Command(kFloatOutput, L"Float", [this] { FloatOutput(); })
                .SetShortcut({FVIRTKEY | FCONTROL | FSHIFT, 'F'}))
            .Add(mwfl::Command(kDockOutput, L"Dock Output", [this] { RedockOutput(); })
                .SetShortcut({FVIRTKEY | FCONTROL | FSHIFT, 'D'}))
            .Add(mwfl::Command(kAutoHideExplorer, L"Auto-hide",
                [this] { AutoHideExplorer(); }))
            .Add(mwfl::Command(kPinExplorer, L"Pin Explorer",
                [this] { PinExplorer(); }))
            .Add(mwfl::Command(kKeyboardDock, L"Keyboard dock",
                [this] { BeginKeyboardDock(); }).SetShortcut({FVIRTKEY | FCONTROL, 'K'}))
            .Add(mwfl::Command(kSaveLayout, L"Save layout",
                [this] { SaveLayout(); }).SetShortcut({FVIRTKEY | FCONTROL, 'S'}))
            .Add(mwfl::Command(kResetLayout, L"Reset Layout",
                [this] { ApplySnapshot(DefaultLayout(), L"Default layout restored"); }))
            .Add(mwfl::Command(kReorderDocuments, L"Move README Tab First",
                [this] { ReorderDocuments(); })
                .SetShortcut({FVIRTKEY | FCONTROL | FSHIFT, 'R'}))
            .Add(mwfl::Command(kCloseReadme, L"Close README (Reset Restores)",
                [this] { CloseReadme(); }).SetShortcut({FVIRTKEY | FCONTROL, 'W'}))
            .Add(mwfl::Command(kExit, L"Exit", [this] { Close(); }));
    }

    void BuildMenu() {
        mwfl::Menu bar, popup;
        mwfl::Must(bar.Create() && popup.CreatePopup(), "create docking menu");
        for (mwfl::ControlId id : {kFloatOutput, kDockOutput, kAutoHideExplorer,
                kPinExplorer, kKeyboardDock, kReorderDocuments, kCloseReadme,
                kSaveLayout, kResetLayout})
            mwfl::Must(popup.AppendCommand(*commands_.Find(id)), "append docking command");
        popup.AppendSeparator();
        mwfl::Must(popup.AppendCommand(*commands_.Find(kExit)), "append exit command");
        mwfl::Must(bar.AppendSubmenu(std::move(popup), L"Workspace") &&
                   bar.AttachToWindow(GetHwnd()), "attach docking menu");
    }

    void CreatePanels() {
        main_editor_ = ::CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT",
            L"// examples/showcase/main.cpp\r\n"
            L"#include <mwfl/mwfl.h>\r\n\r\n"
            L"using mwfl::operator\"\"_dip;\r\n\r\n"
            L"class ShowcaseWindow final : public mwfl::WindowBase {\r\n"
            L"public:\r\n"
            L"    void BuildUI() override {\r\n"
            L"        SetTitle(L\"MWFL Showcase\");\r\n"
            L"        SetLayout(mwfl::Column()\r\n"
            L"            .Margin(24_dip).Gap(12_dip)\r\n"
            L"            .Add(workspace_, mwfl::Stretch()));\r\n"
            L"    }\r\n"
            L"};\r\n",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_MULTILINE | WS_VSCROLL,
            0, 0, 10, 10, documents_.GetHwnd(), reinterpret_cast<HMENU>(1101),
            ::GetModuleHandleW(nullptr), nullptr);
        readme_editor_ = ::CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT",
            L"MWFL 0.1 SHOWCASE\r\n\r\n"
            L"A real-HWND IDE workspace with persistent docking, floating tools, "
            L"auto-hide, mouse previews, and complete keyboard operation.\r\n\r\n"
            L"Try Ctrl+K to dock the Output panel without a mouse.",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_MULTILINE | WS_VSCROLL,
            0, 0, 10, 10, documents_.GetHwnd(), reinterpret_cast<HMENU>(1102),
            ::GetModuleHandleW(nullptr), nullptr);
        explorer_ = ::CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", nullptr,
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | LBS_NOTIFY,
            0, 0, 10, 10, left_.GetHwnd(), reinterpret_cast<HMENU>(1103),
            ::GetModuleHandleW(nullptr), nullptr);
        output_ = ::CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT",
            L"[build] mwfl_showcase  Release | x64\r\n"
            L"[build] Native controls and docking workspace ........ done\r\n"
            L"[test ] Focused GUI verification .................. 12/12\r\n"
            L"[pack ] Public-preview assets ....................... done\r\n"
            L"[done ] Succeeded in 4.8s — 0 warnings, 0 errors\r\n",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_MULTILINE | ES_READONLY | WS_VSCROLL,
            0, 0, 10, 10, bottom_.GetHwnd(), reinterpret_cast<HMENU>(1104),
            ::GetModuleHandleW(nullptr), nullptr);
        mwfl::Must(main_editor_ && readme_editor_ && explorer_ && output_,
                   "create docking panels");
        for (const wchar_t* item : {L"include", L"  mwfl", L"src", L"tests",
                                    L"examples", L"  code_editor", L"  explorer",
                                    L"  docking_workspace", L"docs", L"site"})
            ::SendMessageW(explorer_, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(item));
        for (auto [id, window] : PanelBindings()) {
            documents_.RegisterPanel(id, window);
            left_.RegisterPanel(id, window);
            bottom_.RegisterPanel(id, window);
            floating_group_.RegisterPanel(id, window);
            mwfl::SetAccessibleName(window, model_.FindPanel(id)->title.c_str());
        }
        ApplyPanelFont(GetDpiContext().GetDpi());
    }

    void ApplyPanelFont(UINT dpi) {
        if (!panel_font_.CreateMessageFont(dpi)) return;
        for (const auto [id, window] : PanelBindings()) {
            static_cast<void>(id);
            mwfl::SetControlFont(window, panel_font_.GetHandle());
        }

        LOGFONTW code{};
        if (::GetObjectW(panel_font_.GetHandle(), sizeof(code), &code) == 0) return;
        code.lfPitchAndFamily = FIXED_PITCH | FF_MODERN;
        wcscpy_s(code.lfFaceName, L"Cascadia Mono");
        HFONT next = ::CreateFontIndirectW(&code);
        if (next == nullptr) return;
        for (HWND window : {main_editor_, readme_editor_, output_})
            mwfl::SetControlFont(window, next);
        if (code_font_ != nullptr) ::DeleteObject(code_font_);
        code_font_ = next;
    }

    std::array<std::pair<mwfl::DockPanelId, HWND>, 4> PanelBindings() noexcept {
        return {{{kMainDocument, main_editor_}, {kReadmeDocument, readme_editor_},
                 {kSolutionExplorer, explorer_}, {kOutputPanel, output_}}};
    }

    void BindNativeState() {
        mwfl::Must(static_cast<bool>(native_.Attach(GetHwnd())), "attach docking native adapter");
        for (auto [id, host] : std::array{
                std::pair{kDocuments, documents_.GetHwnd()},
                std::pair{kLeftTools, left_.GetHwnd()},
                std::pair{kBottomTools, bottom_.GetHwnd()},
                std::pair{kFloatingTools, floating_group_.GetHwnd()}})
            mwfl::Must(static_cast<bool>(native_.BindGroup(id, host)), "bind docking group");
        for (auto [id, window] : PanelBindings())
            mwfl::Must(static_cast<bool>(native_.BindPanel(id, window)), "bind docking panel");
    }

    bool EnsureFloating(const mwfl::DockFloatingPlacement& placement =
        {720, 120, 520, 340, L"primary"}) {
        if (!floating_.GetHwnd()) {
            mwfl::DockFloatingWindowOptions options;
            options.title = L"Output — Floating";
            options.placement = placement;
            options.target_dpi = GetDpiContext().GetDpi();
            options.on_close = [this] {
                return RedockOutput() ? mwfl::DockFloatingCloseAction::hide
                                      : mwfl::DockFloatingCloseAction::cancel;
            };
            if (!floating_.Create(GetHwnd(), std::move(options)) ||
                !floating_.AttachContent(floating_group_.GetHwnd())) return false;
        }
        floating_.Show(false);
        return true;
    }

    bool ApplyMutation(const mwfl::DockMutation& mutation, std::wstring_view message) {
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

    bool ApplySnapshot(mwfl::DockLayoutSnapshot snapshot, std::wstring_view message) {
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
        const mwfl::DockFloatingPlacement placement{720, 120, 520, 340, L"primary"};
        if (!EnsureFloating(placement)) return false;
        mwfl::DockMutation mutation;
        mutation.kind = mwfl::DockMutationKind::float_panel;
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
            mwfl::MakePinDockMutation(kOutputPanel, kBottomTools),
            L"Output redocked below documents");
        if (result && floating_.GetHwnd()) {
            floating_.DetachContent();
            floating_.Hide();
        }
        return result;
    }

    bool AutoHideExplorer() {
        if (model_.IsAutoHidden(kSolutionExplorer)) return true;
        mwfl::DockMutation mutation;
        mutation.kind = mwfl::DockMutationKind::auto_hide;
        mutation.panel = kSolutionExplorer;
        mutation.edge = mwfl::DockEdge::left;
        return ApplyMutation(mutation, L"Solution Explorer auto-hidden; Pin restores it");
    }

    bool PinExplorer() {
        if (model_.FindPanelGroup(kSolutionExplorer) == kLeftTools) return true;
        return ApplyMutation(mwfl::MakePinDockMutation(kSolutionExplorer, kLeftTools),
                             L"Solution Explorer pinned");
    }

    bool ReorderDocuments() {
        const auto* group = model_.FindGroup(kDocuments);
        if (!group || group->panels.size() < 2) return false;
        mwfl::DockMutation mutation;
        mutation.kind = mwfl::DockMutationKind::move_to_group;
        mutation.panel = kReadmeDocument;
        mutation.target_group = kDocuments;
        mutation.target_index = 0;
        return ApplyMutation(mutation, L"README tab moved first");
    }

    bool CloseReadme() {
        if (!model_.FindPanel(kReadmeDocument)) return true;
        mwfl::DockMutation mutation;
        mutation.kind = mwfl::DockMutationKind::close_panel;
        mutation.panel = kReadmeDocument;
        return ApplyMutation(mutation, L"README closed; Reset Layout restores it");
    }

    mwfl::DockPointDip ScreenPointDip(POINT point) const noexcept {
        const auto dpi = GetDpiContext();
        return {dpi.FromPixels(point.x).value, dpi.FromPixels(point.y).value};
    }

    std::optional<mwfl::DockMutation> MutationForTarget(
        mwfl::DockPanelId panel, const mwfl::DockDropTarget& target) {
        if (target.kind == mwfl::DockTargetKind::tab_group)
            return mwfl::MakePinDockMutation(panel, target.group);
        if (target.kind == mwfl::DockTargetKind::auto_hide) {
            mwfl::DockMutation mutation;
            mutation.kind = mwfl::DockMutationKind::auto_hide;
            mutation.panel = panel;
            mutation.edge = target.edge;
            return mutation;
        }
        if (target.kind == mwfl::DockTargetKind::floating && panel == kOutputPanel &&
            model_.FindPanelGroup(panel) != kFloatingTools) {
            const mwfl::DockFloatingPlacement placement{720, 120, 520, 340, L"primary"};
            if (!EnsureFloating(placement)) return std::nullopt;
            mwfl::DockMutation mutation;
            mutation.kind = mwfl::DockMutationKind::float_panel;
            mutation.panel = panel;
            mutation.new_group = kFloatingTools;
            mutation.new_group_node = {202};
            mutation.new_floating_host = {400};
            mutation.floating_placement = placement;
            return mutation;
        }
        return std::nullopt;
    }

    void BeginMouseDock(mwfl::DockPanelId panel, POINT point) {
        CancelMouseDock(mwfl::DockDragCancelReason::explicit_cancel);
        mouse_drag_.Reset();
        mouse_targets_ = KeyboardTargets();
        if (panel != kOutputPanel)
            std::erase_if(mouse_targets_, [](const auto& target) {
                return target.kind == mwfl::DockTargetKind::floating;
            });
        if (!mouse_drag_.Begin(model_, panel)) {
            SetStatus(L"Mouse docking could not start");
            return;
        }
        if (!preview_.GetHwnd()) preview_.Create(GetHwnd());
        UpdateMouseDock(point);
    }

    void UpdateMouseDock(POINT point) {
        if (mouse_drag_.GetState() != mwfl::DockDragState::tracking &&
            mouse_drag_.GetState() != mwfl::DockDragState::proposed) return;
        const auto target = mouse_drag_.UpdateTarget(ScreenPointDip(point), mouse_targets_);
        if (!target) {
            preview_.Hide();
            SetStatus(L"Drag over a highlighted docking destination");
            return;
        }
        const auto mutation = MutationForTarget(mouse_drag_.GetPanel(), *target);
        const auto proposal = mutation ? model_.Propose(*mutation) : std::nullopt;
        if (!proposal || !mouse_drag_.SetProposal(*proposal)) {
            preview_.Hide();
            return;
        }
        preview_.Show(target->bounds, GetDpiContext().GetDpi());
        SetStatus(L"Release to dock; Escape or capture loss cancels without changing layout");
    }

    bool EndMouseDock(POINT point) {
        UpdateMouseDock(point);
        preview_.Hide();
        mouse_adoption_.reset();
        const auto result = mouse_drag_.Commit(model_,
            [this](const mwfl::DockLayoutSnapshot& proposed) {
                mouse_adoption_ = native_.Prepare(proposed);
                return mouse_adoption_ &&
                       static_cast<bool>(native_.Adopt(*mouse_adoption_));
            },
            [this](const mwfl::DockLayoutSnapshot&) {
                if (mouse_adoption_) native_.Rollback(*mouse_adoption_);
            });
        if (!result) {
            mouse_adoption_.reset();
            SetStatus(L"Mouse docking cancelled; original layout retained");
            return false;
        }
        if (!mouse_adoption_ || !native_.Commit(*mouse_adoption_))
            throw std::runtime_error("commit mouse docking adoption failed");
        mouse_adoption_.reset();
        Synchronize(L"Panel docked by mouse");
        return true;
    }

    void CancelMouseDock(mwfl::DockDragCancelReason reason) noexcept {
        if (mouse_drag_.GetState() == mwfl::DockDragState::tracking ||
            mouse_drag_.GetState() == mwfl::DockDragState::proposed)
            mouse_drag_.Cancel(reason);
        preview_.Hide();
        mouse_adoption_.reset();
    }

    std::vector<mwfl::DockDropTarget> KeyboardTargets() const {
        std::vector<mwfl::DockDropTarget> result;
        const auto dpi = GetDpiContext();
        const auto add_group = [&](std::uint64_t id, HWND window,
                                   mwfl::DockGroupId group) {
            RECT pixels{};
            ::GetWindowRect(window, &pixels);
            result.push_back({id, mwfl::DockTargetKind::tab_group,
                {dpi.FromPixels(pixels.left).value, dpi.FromPixels(pixels.top).value,
                 dpi.FromPixels(pixels.right - pixels.left).value,
                 dpi.FromPixels(pixels.bottom - pixels.top).value}, group});
        };
        add_group(1, left_.GetHwnd(), kLeftTools);
        add_group(2, bottom_.GetHwnd(), kBottomTools);
        RECT main{};
        ::GetWindowRect(GetHwnd(), &main);
        result.push_back({3, mwfl::DockTargetKind::auto_hide,
            {dpi.FromPixels(main.left).value,
             dpi.FromPixels(main.bottom - 42).value,
             dpi.FromPixels(main.right - main.left).value, 42.0}, {},
             mwfl::DockEdge::bottom});
        result.push_back({4, mwfl::DockTargetKind::floating,
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

    void ApplyKeyboardTarget(const mwfl::DockDropTarget& target) {
        if (target.kind == mwfl::DockTargetKind::tab_group)
            ApplyMutation(mwfl::MakePinDockMutation(kOutputPanel, target.group),
                          L"Output docked by keyboard");
        else if (target.kind == mwfl::DockTargetKind::auto_hide) {
            mwfl::DockMutation mutation;
            mutation.kind = mwfl::DockMutationKind::auto_hide;
            mutation.panel = kOutputPanel;
            mutation.edge = target.edge;
            ApplyMutation(mutation, L"Output auto-hidden by keyboard");
        } else if (target.kind == mwfl::DockTargetKind::floating) {
            FloatOutput();
        }
    }

    void Synchronize(std::wstring_view message) {
        mwfl::Must(static_cast<bool>(native_.Synchronize(model_.GetSnapshot())),
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
        constexpr int toolbar_height = 38;
        constexpr int status_height = 22;
        ::SetWindowPos(toolbar_.GetHwnd(), nullptr, 0, 0, client.right, toolbar_height,
                       SWP_NOZORDER | SWP_NOACTIVATE);
        ::SetWindowPos(status_.GetHwnd(), nullptr, 0, client.bottom - status_height,
                       client.right, status_height, SWP_NOZORDER | SWP_NOACTIVATE);
        const auto dpi = GetDpiContext();
        const int client_width = static_cast<int>(client.right);
        const int workspace_height = (std::max)(0,
            static_cast<int>(client.bottom) - toolbar_height - status_height);
        outer_.SetBounds(mwfl::RectDip{0.0_dip, dpi.FromPixels(toolbar_height),
            dpi.FromPixels(client_width), dpi.FromPixels(workspace_height)});
        LayoutInner();
    }

    void LayoutInner() noexcept {
        if (!right_holder_ || !::IsWindow(right_holder_)) return;
        RECT client{};
        ::GetClientRect(right_holder_, &client);
        const auto dpi = mwfl::DpiContext::FromWindow(right_holder_);
        inner_.SetBounds({0.0_dip, 0.0_dip, dpi.FromPixels(client.right),
                          dpi.FromPixels(client.bottom)});
    }

    std::filesystem::path SessionPath() const {
        if (g_self_test) return std::filesystem::temp_directory_path() /
            (L"mwfl-docking-workspace-" + std::to_wstring(::GetCurrentProcessId()) +
             L".state");
        std::array<wchar_t, 32768> local{};
        const DWORD length = ::GetEnvironmentVariableW(
            L"LOCALAPPDATA", local.data(), static_cast<DWORD>(local.size()));
        if (length == 0 || length >= local.size()) return {};
        return std::filesystem::path{local.data()} / L"mwfl" / L"docking-workspace.state";
    }

    bool SaveLayout() {
        const auto path = SessionPath();
        if (path.empty()) return false;
        std::error_code error;
        std::filesystem::create_directories(path.parent_path(), error);
        const bool saved = mwfl::SaveDockingSessionAtomic(path, model_.GetSnapshot()) ==
                           mwfl::DockingSessionStatus::success;
        SetStatus(saved ? L"Layout saved" : L"Layout save failed");
        return saved;
    }

    void RestoreLayout() {
        const auto path = SessionPath();
        if (path.empty()) return;
        const auto loaded = mwfl::LoadDockingSession(path);
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
        const auto targets = KeyboardTargets();
        const auto left_target = std::find_if(targets.begin(), targets.end(),
            [](const auto& target) { return target.id == 1; });
        const auto bottom_target = std::find_if(targets.begin(), targets.end(),
            [](const auto& target) { return target.id == 2; });
        if (left_target == targets.end() || bottom_target == targets.end())
            throw std::runtime_error("mouse docking targets missing");
        const auto center = [this](const mwfl::DockDropTarget& target) {
            const auto dpi = GetDpiContext();
            return POINT{dpi.ToPixels(mwfl::Dip(static_cast<float>(target.bounds.x +
                                      target.bounds.width / 2.0))),
                         dpi.ToPixels(mwfl::Dip(static_cast<float>(target.bounds.y +
                                      target.bounds.height / 2.0)))};
        };
        BeginMouseDock(kOutputPanel, center(*left_target));
        if (!EndMouseDock(center(*left_target)) ||
            model_.FindPanelGroup(kOutputPanel) != kLeftTools)
            throw std::runtime_error("mouse docking commit failed");
        BeginMouseDock(kOutputPanel, center(*bottom_target));
        CancelMouseDock(mwfl::DockDragCancelReason::escape);
        if (model_.FindPanelGroup(kOutputPanel) != kLeftTools)
            throw std::runtime_error("mouse docking cancellation changed layout");
        if (!RedockOutput()) throw std::runtime_error("mouse test redock failed");
        self_test_step_ = 7;
        if (!ReorderDocuments() || !model_.FindGroup(kDocuments) ||
            model_.FindGroup(kDocuments)->panels.front() != kReadmeDocument)
            throw std::runtime_error("tab reorder workflow failed");
        if (!CloseReadme() || model_.FindPanel(kReadmeDocument) ||
            ::IsWindowVisible(readme_editor_))
            throw std::runtime_error("close panel workflow failed");
        if (!ApplySnapshot(DefaultLayout(), L"Self-test layout restored") ||
            !model_.FindPanel(kReadmeDocument))
            throw std::runtime_error("closed panel restore workflow failed");
        self_test_step_ = 8;
        BeginKeyboardDock();
        if (!keyboard_.IsActive() || !keyboard_.Move(mwfl::DockKeyboardMove::left))
            throw std::runtime_error("keyboard docking navigation failed");
        const auto accepted = keyboard_.Accept();
        preview_.Hide();
        if (!accepted) throw std::runtime_error("keyboard docking accept failed");
        ApplyKeyboardTarget(accepted->target);
        self_test_step_ = 9;
        if (!SaveLayout()) throw std::runtime_error("layout save failed");
        const auto loaded = mwfl::LoadDockingSession(SessionPath());
        if (!loaded || loaded.snapshot->panels.size() != 4)
            throw std::runtime_error("layout restore file failed");
        self_test_step_ = 10;
        const std::array monitors{
            mwfl::DockMonitorWorkArea{L"primary", 0, 0, 1920, 1040, 96, true}};
        if (!mwfl::RecoverDockFloatingPlacement(
                {5000, 5000, 600, 400, L"removed"}, monitors))
            throw std::runtime_error("monitor recovery failed");
        std::error_code ignored;
        std::filesystem::remove(SessionPath(), ignored);
        floating_.Destroy();
        preview_.Destroy();
        native_.Detach();
    }

    mwfl::DockLayoutModel model_;
    mwfl::DockNativeWorkspaceAdapter native_;
    mwfl::DockFloatingWindow floating_;
    mwfl::DockPreviewWindow preview_;
    mwfl::DockKeyboardSession keyboard_;
    mwfl::DockDragSession mouse_drag_;
    std::vector<mwfl::DockDropTarget> mouse_targets_;
    std::optional<mwfl::DockNativeAdoptionPlan> mouse_adoption_;
    std::vector<mwfl::DockDropTarget> keyboard_targets_;
    mwfl::CommandSet commands_;
    mwfl::AcceleratorTable accelerators_;
    mwfl::Toolbar toolbar_;
    mwfl::StatusBar status_;
    mwfl::Splitter outer_;
    mwfl::Splitter inner_;
    GroupView documents_, left_, bottom_, floating_group_;
    HWND right_holder_ = nullptr;
    HWND main_editor_ = nullptr;
    HWND readme_editor_ = nullptr;
    HWND explorer_ = nullptr;
    HWND output_ = nullptr;
    mwfl::UiFont panel_font_;
    HFONT code_font_ = nullptr;
    int self_test_step_ = 1;
};

}  // namespace

int RunDockingWorkspaceExample(HINSTANCE instance, int show) {
    g_self_test = wcsstr(::GetCommandLineW(), L"--self-test") != nullptr;
    g_showcase = wcsstr(::GetCommandLineW(), L"--showcase") != nullptr;
    return mwfl::RunApplication<DockingIdeWindow>(instance, show,
        {.title = L"mwfl Docking IDE Workspace",
         .initial_bounds = {{24.0_dip, 24.0_dip}, {1437.0_dip, 868.5_dip}},
         .use_default_bounds = false});
}
