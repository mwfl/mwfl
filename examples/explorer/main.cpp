#include <mwfl/mwfl.h>

#include "explorer_model.h"

#include <oleacc.h>
#include <windowsx.h>

#include <array>
#include <memory>
#include <ranges>
#include <stdexcept>
#include <string>
#include <vector>

using mwfl::operator""_dip;

namespace {

constexpr UINT kRunSelfTest = WM_APP + 0x140;
constexpr mwfl::ControlId kBack{700};
constexpr mwfl::ControlId kUp{701};
constexpr mwfl::ControlId kRefresh{702};
constexpr mwfl::ControlId kSortName{703};
constexpr mwfl::ControlId kOpen{710};
constexpr mwfl::ControlId kProperties{711};
bool g_self_test = false;

bool HasAccessibleName(HWND window, std::wstring_view expected) {
    IAccessible* accessible = nullptr;
    if (FAILED(::AccessibleObjectFromWindow(window, static_cast<DWORD>(OBJID_CLIENT), IID_IAccessible,
                                            reinterpret_cast<void**>(&accessible))))
        return false;
    VARIANT child{};
    child.vt = VT_I4;
    child.lVal = CHILDID_SELF;
    BSTR name = nullptr;
    const bool matches = SUCCEEDED(accessible->get_accName(child, &name)) && name != nullptr &&
                         std::wstring_view{name, ::SysStringLen(name)} == expected;
    if (name != nullptr) ::SysFreeString(name);
    accessible->Release();
    return matches;
}

class ExplorerWindow final : public mwfl::WindowBase {
public:
    void BuildUI() override {
        SetTitle(L"mwfl Explorer");
        BuildImagesAndCommands();

        mwfl::ControlHost ui{*this};
        ui.Add(rebar_, {200}, {0.0_dip, 0.0_dip, 980.0_dip, 42.0_dip});
        mwfl::ControlHost rebar_ui{rebar_};
        rebar_ui.Add(toolbar_, {201}, {0.0_dip, 0.0_dip, 520.0_dip, 34.0_dip});
        ui.Add(tabs_, {202}, {0.0_dip, 42.0_dip, 980.0_dip, 34.0_dip});
        ui.Add(splitter_, {203}, {0.0_dip, 76.0_dip, 980.0_dip, 540.0_dip},
               mwfl::SplitterOptions{.constraints = {180.0_dip, 320.0_dip, 6.0_dip},
                                     .initial_position = 250.0_dip});
        mwfl::ControlHost panes{splitter_};
        panes.Add(tree_, {204}, mwfl::RectDip{});
        panes.Add(list_, {205}, mwfl::RectDip{},
                  mwfl::ListViewOptions{.virtual_data = true});
        ui.Add(status_, {206}, {0.0_dip, 616.0_dip, 980.0_dip, 26.0_dip});

        mwfl::Must(splitter_.AttachPanes(tree_.GetHwnd(), list_.GetHwnd()),
                   "attach Explorer panes");
        mwfl::Must(toolbar_.SetImageList(images_), "attach toolbar image list");
        for (const auto& command : commands_.GetCommands()) {
            if (command.GetId() == kOpen || command.GetId() == kProperties) continue;
            mwfl::Must(toolbar_.AddCommand(command), "add Explorer toolbar command");
        }
        toolbar_.AutoSize();
        mwfl::Must(rebar_.AddBand(toolbar_, L"Navigation", 500), "add Explorer rebar band");
        mwfl::Must(accelerators_.Create(commands_), "create Explorer accelerators");
        SetAccelerators(accelerators_.GetHandle());

        mwfl::Must(mwfl::AddColumns(list_, {{L"Name", 300}, {L"Type", 210}, {L"Size", 120}}),
                   "add Explorer columns");
        list_.SetExtendedListStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER |
                                   LVS_EX_HEADERDRAGDROP);
        ListView_SetImageList(list_.GetHwnd(), images_.GetHandle(), LVSIL_SMALL);
        TreeView_SetImageList(tree_.GetHwnd(), images_.GetHandle(), TVSIL_NORMAL);
        PopulateTree();
        mwfl::Must(list_.SetVirtualModel(model_), "attach Explorer virtual model");

        mwfl::Must(tab_model_.Add({{1}, L"All files", false, false}), "add All files tab");
        mwfl::Must(tab_model_.Add({{2}, L"Pictures", false, false}), "add Pictures tab");
        mwfl::Must(tabs_.Synchronize(tab_model_), "synchronize Explorer tabs");

        mwfl::Must(mwfl::SetAccessibleName(toolbar_.GetHwnd(), L"Explorer commands"),
                   "name Explorer toolbar");
        mwfl::Must(mwfl::SetAccessibleName(tree_.GetHwnd(), L"Folders"),
                   "name Explorer tree");
        mwfl::Must(mwfl::SetAccessibleName(list_.GetHwnd(), L"Folder contents"),
                   "name Explorer list");
        mwfl::Must(mwfl::SetAccessibleName(tabs_.GetHwnd(), L"Explorer views"),
                   "name Explorer tabs");

        SetLayout(mwfl::Column()
                      .Add(rebar_, mwfl::Auto())
                      .Add(tabs_, mwfl::Fixed(34.0_dip))
                      .Add(splitter_, mwfl::Stretch())
                      .Add(status_, mwfl::Auto()));
        UpdateStatusParts(980);
        UpdateStatus(L"Ready");
        tree_.SetSelection({1});
        if (g_self_test && ::PostMessageW(GetHwnd(), kRunSelfTest, 0, 0) == FALSE)
            throw std::runtime_error("post Explorer self-test message failed");
    }

    mwfl::EventResult OnCommand(const mwfl::CommandEvent& event) override {
        return commands_.Dispatch(event);
    }

    mwfl::EventResult OnNotify(const mwfl::NotifyEvent& event) override {
        if (event.IsFrom(list_)) {
            LRESULT result = 0;
            if (list_.HandleNotification(event.header, result)) {
                if (auto error = list_.TakeVirtualException()) std::rethrow_exception(error);
                return mwfl::EventResult::Handled(result);
            }
            if (const auto notification = list_.DecodeNotification(event.header)) {
                if (notification->kind == mwfl::ListViewNotificationKind::column_clicked) {
                    SortByColumn(notification->column);
                    return mwfl::EventResult::Handled();
                }
                if (notification->kind == mwfl::ListViewNotificationKind::selection_changed) {
                    UpdateSelectionStatus();
                    return mwfl::EventResult::Handled();
                }
            }
        }
        if (event.IsFrom(tree_)) {
            if (const auto notification = tree_.DecodeNotification(event.header);
                notification &&
                notification->kind == mwfl::TreeViewNotificationKind::selection_changed) {
                Navigate(notification->item, !suppress_history_);
                return mwfl::EventResult::Handled();
            }
        }
        if (event.IsFrom(tabs_) && event.header.code == TCN_SELCHANGE) {
            const auto selected = tabs_.GetSelectedTabId();
            tree_.SetSelection(selected == mwfl::TabId{2} ? mwfl::TreeItemId{20}
                                                          : mwfl::TreeItemId{1});
            status_.SetPartText(2, selected == mwfl::TabId{2} ? L"Pictures filter"
                                                              : L"All files filter");
            return mwfl::EventResult::Handled();
        }
        if (event.Is(splitter_, mwfl::kSplitterPositionChanged)) {
            status_.SetPartText(2, L"Pane width changed");
            return mwfl::EventResult::Handled();
        }
        return mwfl::EventResult::Propagate();
    }

    mwfl::EventResult OnResize(const mwfl::ResizeEvent& event) override {
        UpdateStatusParts(event.client_size.cx);
        return mwfl::EventResult::Propagate();
    }

    mwfl::EventResult OnMessage(const mwfl::WindowMessage& event) override {
        if (event.id == WM_THEMECHANGED || event.id == WM_SETTINGCHANGE) {
            static_cast<void>(mwfl::ApplyWindowAppearance(GetHwnd()));
            ::RedrawWindow(GetHwnd(), nullptr, nullptr,
                           RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
            return mwfl::EventResult::Handled();
        }
        if (event.id == WM_CONTEXTMENU) {
            const HWND source = reinterpret_cast<HWND>(event.wparam);
            if (source == tree_.GetHwnd() || source == list_.GetHwnd()) {
                ShowContextMenu(source, {GET_X_LPARAM(event.lparam), GET_Y_LPARAM(event.lparam)});
                return mwfl::EventResult::Handled();
            }
        }
        if (event.id == kRunSelfTest) {
            try {
                RunSelfTest();
            } catch (...) {
                ::PostQuitMessage(self_test_step_);
            }
            return mwfl::EventResult::Handled();
        }
        return mwfl::EventResult::Propagate();
    }

private:
    void BuildImagesAndCommands() {
        mwfl::Must(images_.Create(16, 16), "create Explorer image list");
        for (const wchar_t* icon_name : {IDI_APPLICATION, IDI_INFORMATION, IDI_ASTERISK,
                                        IDI_WARNING}) {
            mwfl::Must(images_.AddIcon(::LoadIconW(nullptr, icon_name)) >= 0,
                       "add Explorer image");
        }
        commands_.Add(mwfl::Command{kBack, L"Back", [this] { GoBack(); }}
                          .SetImageIndex(0)
                          .SetShortcut({FALT | FVIRTKEY, VK_LEFT}));
        commands_.Add(mwfl::Command{kUp, L"Up", [this] { GoUp(); }}
                          .SetImageIndex(1)
                          .SetShortcut({FALT | FVIRTKEY, VK_UP}));
        commands_.Add(mwfl::Command{kRefresh, L"Refresh", [this] { Refresh(); }}
                          .SetImageIndex(2)
                          .SetShortcut({FVIRTKEY, VK_F5}));
        commands_.Add(mwfl::Command{kSortName, L"Sort", [this] { ToggleNameSort(); }}
                          .SetImageIndex(3));
        commands_.Add(mwfl::Command{kOpen, L"Open", [this] { OpenSelection(); }});
        commands_.Add(mwfl::Command{kProperties, L"Properties", [this] { ShowProperties(); }});
    }

    void PopulateTree() {
        const auto folders = model_->GetFolders();
        for (const auto& folder : folders) {
            HTREEITEM item = folder.parent ? tree_.AddChild(folder.id, folder.name, folder.parent)
                                           : tree_.AddItem(folder.id, folder.name);
            mwfl::Must(item != nullptr, "add Explorer folder");
            TVITEMW native{};
            native.mask = TVIF_IMAGE | TVIF_SELECTEDIMAGE;
            native.hItem = item;
            native.iImage = folder.parent ? 1 : 0;
            native.iSelectedImage = native.iImage;
            mwfl::Must(TreeView_SetItem(tree_.GetHwnd(), &native) != FALSE,
                       "set Explorer folder image");
        }
        mwfl::Must(tree_.Expand({1}), "expand Explorer root");
    }

    void Navigate(mwfl::TreeItemId folder, bool record_history) {
        if (!folder || folder == model_->GetSelectedFolder()) return;
        if (record_history) history_.push_back(model_->GetSelectedFolder());
        mwfl::Must(list_.UpdateVirtualModel([&] {
                       mwfl::Must(model_->SelectFolder(folder), "select Explorer folder");
                   }),
                   "refresh Explorer folder");
        tabs_.SetSelection(folder == mwfl::TreeItemId{20} ? mwfl::TabId{2}
                                                          : mwfl::TabId{1});
        ListView_SetItemState(list_.GetHwnd(), -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
        UpdateStatus(L"Folder changed");
    }

    void GoBack() {
        if (history_.empty()) {
            UpdateStatus(L"No previous folder");
            return;
        }
        const auto folder = history_.back();
        history_.pop_back();
        suppress_history_ = true;
        tree_.SetSelection(folder);
        suppress_history_ = false;
    }

    void GoUp() {
        const auto current = model_->GetSelectedFolder();
        const auto folders = model_->GetFolders();
        const auto found = std::ranges::find(folders, current, &explorer_example::FolderEntry::id);
        if (found != folders.end() && found->parent) tree_.SetSelection(found->parent);
    }

    void Refresh() {
        mwfl::Must(list_.RefreshVirtualModel(), "refresh Explorer list");
        UpdateStatus(L"Refreshed");
    }

    void ToggleNameSort() {
        name_ascending_ = !name_ascending_;
        mwfl::Must(list_.UpdateVirtualModel(
                       [&] { model_->Sort(explorer_example::FileColumn::name, name_ascending_); }),
                   "sort Explorer list");
        UpdateStatus(name_ascending_ ? L"Sorted name ascending" : L"Sorted name descending");
    }

    void SortByColumn(int column) {
        const auto selected = column == 1 ? explorer_example::FileColumn::type
                              : column == 2 ? explorer_example::FileColumn::size
                                            : explorer_example::FileColumn::name;
        mwfl::Must(list_.UpdateVirtualModel([&] { model_->Sort(selected, true); }),
                   "sort Explorer column");
        UpdateStatus(L"Column sorted");
    }

    void OpenSelection() {
        if (context_source_ == tree_.GetHwnd()) {
            status_.SetPartText(2, L"Folder is already open");
            return;
        }
        const auto selected = list_.GetSelectedItemIds();
        status_.SetPartText(2, selected.empty() ? L"Select an item to open" : L"Open command received");
    }

    void ShowProperties() {
        if (context_source_ == tree_.GetHwnd()) {
            if (!g_self_test)
                mwfl::ShowTaskDialog(GetHwnd(), L"mwfl Explorer", L"Folder properties",
                                     L"Folder identity is a stable TreeItemId.");
            return;
        }
        const auto selected = list_.GetSelectedItemIds();
        if (selected.empty()) {
            UpdateStatus(L"Select an item for properties");
            return;
        }
        mwfl::ShowTaskDialog(GetHwnd(), L"mwfl Explorer", L"Item properties",
                             L"Stable file identity remains application-owned.");
    }

    void ShowContextMenu(HWND source, POINT position) {
        mwfl::Menu menu;
        mwfl::Must(menu.CreatePopup(), "create Explorer context menu");
        mwfl::Must(menu.AppendCommand(kOpen.value, L"Open"), "add Open context command");
        mwfl::Must(menu.AppendCommand(kProperties.value, L"Properties"),
                   "add Properties context command");
        context_source_ = source;
        context_menu_seen_ = ::GetMenuItemCount(menu.GetHandle()) == 2;
        if (g_self_test) return;
        if (position.x == -1 && position.y == -1) ::GetCursorPos(&position);
        ::SetForegroundWindow(GetHwnd());
        const auto selected = menu.TrackResult(GetHwnd(), position);
        if (selected) ::PostMessageW(GetHwnd(), WM_COMMAND, selected.command, 0);
        else if (!selected.Cancelled()) UpdateStatus(L"Context menu failed");
    }

    void UpdateSelectionStatus() {
        const auto selected = list_.GetSelectedItemIds();
        status_.SetPartText(2, std::to_wstring(selected.size()) + L" selected");
    }

    void UpdateStatus(std::wstring_view action) {
        status_.SetPartText(0, std::to_wstring(model_->GetRowCount()) + L" items");
        const auto folders = model_->GetFolders();
        const auto current = std::ranges::find(folders, model_->GetSelectedFolder(),
                                               &explorer_example::FolderEntry::id);
        status_.SetPartText(1, current == folders.end() ? L"Unknown" : current->name);
        status_.SetPartText(2, action);
    }

    void UpdateStatusParts(LONG width) {
        if (!status_.IsWindow() || width <= 0) return;
        const std::array<int, 3> parts{static_cast<int>(width / 4),
                                       static_cast<int>(width / 2), -1};
        status_.SetParts(parts);
    }

    void RunSelfTest() {
        self_test_step_ = 2;
        if (static_cast<int>(::SendMessageW(rebar_.GetHwnd(), RB_GETBANDCOUNT, 0, 0)) != 1 ||
            static_cast<int>(::SendMessageW(toolbar_.GetHwnd(), TB_BUTTONCOUNT, 0, 0)) != 4 ||
            tabs_.GetSelectedTabId() != mwfl::TabId{1} || model_->GetRowCount() != 6 ||
            ListView_GetImageList(list_.GetHwnd(), LVSIL_SMALL) != images_.GetHandle() ||
            TreeView_GetImageList(tree_.GetHwnd(), TVSIL_NORMAL) != images_.GetHandle())
            throw std::runtime_error("Explorer native composition mismatch");
        self_test_step_ = 3;
        if (!HasAccessibleName(toolbar_.GetHwnd(), L"Explorer commands") ||
            !HasAccessibleName(tree_.GetHwnd(), L"Folders") ||
            !HasAccessibleName(list_.GetHwnd(), L"Folder contents") ||
            !HasAccessibleName(tabs_.GetHwnd(), L"Explorer views"))
            throw std::runtime_error("Explorer accessible names mismatch");

        self_test_step_ = 4;
        tree_.SetSelection({20});
        if (model_->GetSelectedFolder() != mwfl::TreeItemId{20}) {
            self_test_step_ = 41;
            throw std::runtime_error("Explorer tree notification did not update the model");
        }
        if (model_->GetRowCount() != 2) {
            self_test_step_ = 42;
            throw std::runtime_error("Explorer folder filter count mismatch");
        }
        if (list_.GetItemId(0) != mwfl::ListItemId{2001}) {
            self_test_step_ = 43;
            throw std::runtime_error("Explorer virtual list mapping mismatch");
        }
        self_test_step_ = 5;
        ::SendMessageW(GetHwnd(), WM_COMMAND, kSortName.value, 0);
        if (list_.GetItemId(0) != mwfl::ListItemId{2002})
            throw std::runtime_error("Explorer stable sort mismatch");
        self_test_step_ = 6;
        mwfl::Must(list_.SetSelected({2002}), "select Explorer self-test row");
        const DWORD user_before = ::GetGuiResources(::GetCurrentProcess(), GR_USEROBJECTS);
        const DWORD gdi_before = ::GetGuiResources(::GetCurrentProcess(), GR_GDIOBJECTS);
        for (int iteration = 0; iteration < 40; ++iteration) {
            context_menu_seen_ = false;
            ::SendMessageW(GetHwnd(), WM_CONTEXTMENU,
                           reinterpret_cast<WPARAM>(list_.GetHwnd()), -1);
            if (!context_menu_seen_)
                throw std::runtime_error("Explorer context menu was not routed");
        }
        if (::GetGuiResources(::GetCurrentProcess(), GR_USEROBJECTS) > user_before + 4 ||
            ::GetGuiResources(::GetCurrentProcess(), GR_GDIOBJECTS) > gdi_before + 4) {
            self_test_step_ = 61;
            throw std::runtime_error("Explorer context menu leaked GUI resources");
        }

        MSG accelerator{GetHwnd(), WM_KEYDOWN, VK_F5, 0};
        wchar_t status_text[64]{};
        if (::TranslateAcceleratorW(GetHwnd(), accelerators_.GetHandle(), &accelerator) == 0 ||
            ::SendMessageW(status_.GetHwnd(), SB_GETTEXTW, 2,
                           reinterpret_cast<LPARAM>(status_text)) == 0 ||
            std::wstring_view{status_text} != L"Refreshed") {
            self_test_step_ = 62;
            throw std::runtime_error("Explorer F5 accelerator did not dispatch Refresh");
        }

        self_test_step_ = 63;
        if (tabs_.GetSelectedTabId() != mwfl::TabId{2})
            throw std::runtime_error("Explorer tree did not synchronize Pictures tab");
        mwfl::Must(tabs_.SetSelection(mwfl::TabId{1}), "select Explorer All files tab");
        NMHDR tab_change{tabs_.GetHwnd(), static_cast<UINT_PTR>(tabs_.GetId().value), TCN_SELCHANGE};
        ::SendMessageW(GetHwnd(), WM_NOTIFY, tab_change.idFrom,
                       reinterpret_cast<LPARAM>(&tab_change));
        self_test_step_ = 64;
        if (model_->GetSelectedFolder() != mwfl::TreeItemId{1} || model_->GetRowCount() != 6)
            throw std::runtime_error("Explorer All files tab did not navigate");
        ::SendMessageW(GetHwnd(), WM_THEMECHANGED, 0, 0);
        ::SendMessageW(GetHwnd(), WM_SETTINGCHANGE, 0, 0);
        self_test_step_ = 7;
        RECT first{};
        RECT second{};
        if (::GetWindowRect(tree_.GetHwnd(), &first) == FALSE ||
            ::GetWindowRect(list_.GetHwnd(), &second) == FALSE || first.right > second.left ||
            first.bottom <= first.top || second.bottom <= second.top)
            throw std::runtime_error("Explorer splitter geometry mismatch");
        self_test_step_ = 8;
        if (::PostMessageW(GetHwnd(), WM_CLOSE, 0, 0) == FALSE)
            throw std::runtime_error("post Explorer self-test close failed");
    }

    mwfl::ImageList images_;  // Declared first: every borrowing HWND dies first.
    std::shared_ptr<explorer_example::ExplorerModel> model_ =
        std::make_shared<explorer_example::ExplorerModel>();
    mwfl::CommandSet commands_;
    mwfl::AcceleratorTable accelerators_;
    mwfl::Rebar rebar_;
    mwfl::Toolbar toolbar_;
    mwfl::TabControl tabs_;
    mwfl::TabWorkspaceModel tab_model_;
    mwfl::Splitter splitter_;
    mwfl::TreeView tree_;
    mwfl::ListView list_;
    mwfl::StatusBar status_;
    std::vector<mwfl::TreeItemId> history_;
    bool suppress_history_ = false;
    bool name_ascending_ = true;
    bool context_menu_seen_ = false;
    HWND context_source_ = nullptr;
    int self_test_step_ = 1;
};

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    g_self_test = wcsstr(::GetCommandLineW(), L"--self-test") != nullptr;
    return mwfl::RunApplication<ExplorerWindow>(
        instance, show,
        {.title = L"mwfl Explorer",
         .initial_bounds = {{0.0_dip, 0.0_dip}, {1000.0_dip, 680.0_dip}},
         .use_default_bounds = false});
}
