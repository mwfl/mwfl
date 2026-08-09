#include <mwtl/mwtl.h>

#include <windows.h>
#include <commctrl.h>

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct TestState {
    mwtl::ListView* virtual_list = nullptr;
    std::vector<UINT> notifications;
    bool accept_label_edits = false;
};

LRESULT CALLBACK OwnerProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    auto* state = reinterpret_cast<TestState*>(::GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto& create = *reinterpret_cast<const CREATESTRUCTW*>(lparam);
        state = static_cast<TestState*>(create.lpCreateParams);
        ::SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
    } else if (message == WM_NOTIFY && state != nullptr) {
        const auto& header = *reinterpret_cast<const NMHDR*>(lparam);
        state->notifications.push_back(header.code);
        if (state->virtual_list != nullptr) {
            LRESULT result = 0;
            if (state->virtual_list->HandleNotification(header, result)) return result;
        }
        if (state->accept_label_edits &&
            (header.code == TVN_ENDLABELEDITW || header.code == LVN_ENDLABELEDITW))
            return TRUE;
    }
    return ::DefWindowProcW(window, message, wparam, lparam);
}

HWND CreateOwner(TestState& state) {
    WNDCLASSEXW type{sizeof(type)};
    type.lpfnWndProc = OwnerProc;
    type.hInstance = ::GetModuleHandleW(nullptr);
    type.lpszClassName = L"mwtl.navigation.native.owner";
    static_cast<void>(::RegisterClassExW(&type));
    return ::CreateWindowExW(0, type.lpszClassName, L"Navigation native test",
                             WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                             720, 480, nullptr, nullptr, type.hInstance, &state);
}

bool Saw(const TestState& state, UINT code) {
    return std::ranges::find(state.notifications, code) != state.notifications.end();
}

std::wstring TreeText(HWND tree, HTREEITEM handle) {
    wchar_t text[64]{};
    TVITEMW item{};
    item.mask = TVIF_TEXT;
    item.hItem = handle;
    item.pszText = text;
    item.cchTextMax = 64;
    return TreeView_GetItem(tree, &item) ? text : L"";
}

class SampleVirtualModel final : public mwtl::VirtualListModel {
public:
    std::size_t GetRowCount() const noexcept override { return ids.size(); }
    mwtl::ListItemId GetRowId(std::size_t row) const noexcept override {
        return row < ids.size() ? ids[row] : mwtl::ListItemId{};
    }
    std::wstring GetCellText(std::size_t row, int column) const override {
        if (throw_on_text) throw std::runtime_error("virtual text");
        if (row >= names.size()) return {};
        return column == 0 ? names[row] : L"detail " + std::to_wstring(ids[row].value);
    }
    int GetStateImageIndex(std::size_t row) const noexcept override {
        return row == 1 ? 2 : 1;
    }

    std::vector<mwtl::ListItemId> ids{{501}, {502}, {503}};
    std::vector<std::wstring> names{L"alpha", L"beta", L"gamma"};
    bool throw_on_text = false;
};

}  // namespace

int main() {
    using namespace mwtl;
    using mwtl::operator""_dip;

    TestState state;
    TreeView empty_tree;
    ListView empty_list;
    if (empty_tree.SetSelection({1}) || ::GetLastError() != ERROR_INVALID_WINDOW_HANDLE ||
        empty_list.SetSelected({1}) || ::GetLastError() != ERROR_INVALID_WINDOW_HANDLE)
        return 25;
    const HWND owner = CreateOwner(state);
    if (owner == nullptr) return 1;
    ::ShowWindow(owner, SW_SHOWNOACTIVATE);

    TreeView tree;
    TreeViewOptions tree_options;
    tree_options.style |= TVS_EDITLABELS | TVS_CHECKBOXES;
    if (!tree.Create(owner, {101}, {8.0_dip, 8.0_dip, 220.0_dip, 350.0_dip}, tree_options))
        return 2;
    const HTREEITEM item30 = tree.AddItem({30}, L"thirty");
    const HTREEITEM item10 = tree.AddItem({10}, L"ten");
    const HTREEITEM item20 = tree.AddItem({20}, L"twenty");
    const HTREEITEM child = tree.AddChild({21}, L"child", {20});
    if (!item30 || !item10 || !item20 || !child || tree.AddItem({20}, L"duplicate") ||
        ::GetLastError() != ERROR_INVALID_PARAMETER || tree.GetItemId(item10) != TreeItemId{10} ||
        tree.FindItem({20}) != item20)
        return 3;
    if (!tree.SetSelection({20}) || tree.GetSelectedItemId() != TreeItemId{20} ||
        !Saw(state, TVN_SELCHANGEDW) || !tree.SetItemText({20}, L"twenty updated") ||
        TreeText(tree.GetHwnd(), item20) != L"twenty updated")
        return 4;
    if (!tree.SetStateImage({10}, 2) ||
        (TreeView_GetItemState(tree.GetHwnd(), item10, TVIS_STATEIMAGEMASK) &
         TVIS_STATEIMAGEMASK) != INDEXTOSTATEIMAGEMASK(2))
        return 5;
    if (!tree.SortChildren(TVI_ROOT, [](TreeItemId left, TreeItemId right) {
            return left.value < right.value;
        }) || tree.GetItemId(TreeView_GetRoot(tree.GetHwnd())) != TreeItemId{10})
        return 6;
    bool tree_exception = false;
    try {
        static_cast<void>(tree.SortChildren(TVI_ROOT, [](TreeItemId, TreeItemId) -> bool {
            throw std::runtime_error("tree sort");
        }));
    } catch (const std::runtime_error&) {
        tree_exception = true;
    }
    if (!tree_exception) return 7;
    state.notifications.clear();
    if (!tree.BeginLabelEdit({20})) return 80;
    if (TreeView_GetEditControl(tree.GetHwnd()) == nullptr) return 81;
    if (!Saw(state, TVN_BEGINLABELEDITW)) return 82;
    state.accept_label_edits = true;
    ::SetWindowTextW(TreeView_GetEditControl(tree.GetHwnd()), L"tree committed");
    if (!tree.EndLabelEdit(false) || TreeText(tree.GetHwnd(), item20) != L"tree committed" ||
        !Saw(state, TVN_ENDLABELEDITW))
        return 83;
    state.accept_label_edits = false;
    if (!tree.BeginLabelEdit({20}) || !tree.EndLabelEdit(true)) return 84;

    NMTREEVIEWW tree_event{};
    tree_event.hdr.hwndFrom = tree.GetHwnd();
    tree_event.hdr.code = TVN_SELCHANGEDW;
    tree_event.itemOld.lParam = 10;
    tree_event.itemNew.lParam = 20;
    const auto decoded_tree = tree.DecodeNotification(tree_event.hdr);
    if (!decoded_tree || decoded_tree->kind != TreeViewNotificationKind::selection_changed ||
        decoded_tree->previous_item != TreeItemId{10} || decoded_tree->item != TreeItemId{20})
        return 9;
    wchar_t tree_label[] = L"decoded tree";
    NMTVDISPINFOW tree_edit{};
    tree_edit.hdr.hwndFrom = tree.GetHwnd();
    tree_edit.hdr.code = TVN_ENDLABELEDITW;
    tree_edit.item.lParam = 20;
    tree_edit.item.pszText = tree_label;
    const auto decoded_tree_edit = tree.DecodeNotification(tree_edit.hdr);
    NMTVITEMCHANGE tree_state{};
    tree_state.hdr.hwndFrom = tree.GetHwnd();
    tree_state.hdr.code = TVN_ITEMCHANGEDW;
    tree_state.lParam = 10;
    tree_state.uStateNew = TVIS_SELECTED;
    const auto decoded_tree_state = tree.DecodeNotification(tree_state.hdr);
    if (!decoded_tree_edit || decoded_tree_edit->label != L"decoded tree" ||
        !decoded_tree_state || decoded_tree_state->kind != TreeViewNotificationKind::state_changed ||
        decoded_tree_state->item != TreeItemId{10})
        return 90;

    ListView list;
    ListViewOptions list_options;
    list_options.style |= LVS_EDITLABELS;
    if (!list.Create(owner, {102}, {240.0_dip, 8.0_dip, 450.0_dip, 170.0_dip}, list_options) ||
        list.AddColumn(L"Name", 160) < 0 || list.AddColumn(L"Detail", 180) < 0)
        return 10;
    if (list.AddItem({30}, L"thirty") < 0 || list.AddItem({10}, L"ten") < 0 ||
        list.AddItem({20}, L"twenty") < 0 || list.AddItem({20}, L"duplicate") >= 0 ||
        !list.SetItemText({20}, 1, L"detail"))
        return 11;
    state.notifications.clear();
    if (!list.SetSelected({10}) || !list.SetSelected({20}) ||
        list.GetSelectedItemIds().size() != 2 || !Saw(state, LVN_ITEMCHANGED))
        return 12;
    if (!list.SetSelected({30}, true, true) || list.GetSelectedItemIds() !=
            std::vector<ListItemId>{{30}} || !list.SetStateImage({30}, 2))
        return 13;
    if (!list.SortItems([](ListItemId left, ListItemId right) {
            return left.value < right.value;
        }) || list.GetItemId(0) != ListItemId{10})
        return 14;
    state.notifications.clear();
    list.Focus();
    if (!list.BeginLabelEdit({20})) return 150;
    if (ListView_GetEditControl(list.GetHwnd()) == nullptr) return 151;
    if (!Saw(state, LVN_BEGINLABELEDITW)) return 152;
    state.accept_label_edits = true;
    ::SetWindowTextW(ListView_GetEditControl(list.GetHwnd()), L"list committed");
    ::SetFocus(owner);
    wchar_t committed_text[64]{};
    ListView_GetItemText(list.GetHwnd(), *list.FindItem({20}), 0, committed_text, 64);
    if (std::wstring_view(committed_text) != L"list committed" ||
        !Saw(state, LVN_ENDLABELEDITW))
        return 153;
    state.accept_label_edits = false;
    list.Focus();
    if (!list.BeginLabelEdit({20}) || !list.CancelLabelEdit()) return 154;

    NMLISTVIEW list_event{};
    list_event.hdr.hwndFrom = list.GetHwnd();
    list_event.hdr.code = LVN_ITEMCHANGED;
    list_event.iItem = *list.FindItem({20});
    list_event.uChanged = LVIF_STATE;
    list_event.uNewState = LVIS_SELECTED;
    const auto decoded_list = list.DecodeNotification(list_event.hdr);
    if (!decoded_list || decoded_list->kind != ListViewNotificationKind::selection_changed ||
        decoded_list->item != ListItemId{20})
        return 16;
    NMLISTVIEW column_event{};
    column_event.hdr.hwndFrom = list.GetHwnd();
    column_event.hdr.code = LVN_COLUMNCLICK;
    column_event.iSubItem = 1;
    const auto decoded_column = list.DecodeNotification(column_event.hdr);
    NMLVCACHEHINT cache_event{};
    cache_event.hdr.hwndFrom = list.GetHwnd();
    cache_event.hdr.code = LVN_ODCACHEHINT;
    cache_event.iFrom = 4;
    cache_event.iTo = 9;
    const auto decoded_cache = list.DecodeNotification(cache_event.hdr);
    if (!decoded_column || decoded_column->kind != ListViewNotificationKind::column_clicked ||
        decoded_column->column != 1 || !decoded_cache ||
        decoded_cache->kind != ListViewNotificationKind::virtual_cache_hint ||
        decoded_cache->cache_first != 4 || decoded_cache->cache_last != 9)
        return 160;

    ListView virtual_list;
    if (!virtual_list.Create(owner, {103}, {240.0_dip, 190.0_dip, 450.0_dip, 170.0_dip},
                             {.style = list_options.style, .extended_style = WS_EX_CLIENTEDGE,
                              .virtual_data = true}) ||
        virtual_list.AddColumn(L"Name", 160) < 0 || virtual_list.AddColumn(L"Detail", 180) < 0)
        return 17;
    auto model = std::make_shared<SampleVirtualModel>();
    state.virtual_list = &virtual_list;
    if (!virtual_list.SetVirtualModel(model) || ListView_GetItemCount(virtual_list.GetHwnd()) != 3 ||
        virtual_list.GetItemId(1) != ListItemId{502} || virtual_list.FindItem({503}) != 2)
        return 18;
    wchar_t virtual_text[64]{};
    ListView_GetItemText(virtual_list.GetHwnd(), 1, 0, virtual_text, 64);
    if (std::wstring_view(virtual_text) != L"beta") return 19;
    model->throw_on_text = true;
    virtual_text[0] = L'\0';
    ListView_GetItemText(virtual_list.GetHwnd(), 0, 0, virtual_text, 64);
    model->throw_on_text = false;
    if (!virtual_list.TakeVirtualException()) return 190;
    if (!virtual_list.SetSelected({501}) || !virtual_list.SetSelected({503}) ||
        virtual_list.GetSelectedItemIds().size() != 2 ||
        virtual_list.AddItem({900}, L"invalid") >= 0 ||
        virtual_list.SetSubItem(0, 0, L"invalid") || virtual_list.RemoveItem({501}) ||
        virtual_list.SetStateImage({501}, 2) ||
        virtual_list.SortItems([](ListItemId, ListItemId) { return false; }))
        return 20;
    if (!virtual_list.UpdateVirtualModel([&] {
            std::ranges::reverse(model->ids);
            std::ranges::reverse(model->names);
        }) || virtual_list.FindItem({501}) != 2)
        return 200;
    const auto preserved_selection = virtual_list.GetSelectedItemIds();
    if (preserved_selection.size() != 2 ||
        std::ranges::find(preserved_selection, ListItemId{501}) == preserved_selection.end() ||
        std::ranges::find(preserved_selection, ListItemId{503}) == preserved_selection.end())
        return 202;
    bool update_exception = false;
    try {
        static_cast<void>(virtual_list.UpdateVirtualModel([] {
            throw std::runtime_error("update");
        }));
    } catch (const std::runtime_error&) {
        update_exception = true;
    }
    if (!update_exception) return 201;
    auto duplicate_model = std::make_shared<SampleVirtualModel>();
    duplicate_model->ids[2] = duplicate_model->ids[1];
    if (virtual_list.SetVirtualModel(duplicate_model) || ::GetLastError() != ERROR_INVALID_DATA ||
        virtual_list.GetVirtualModel() != model)
        return 21;

    const DWORD user_before = ::GetGuiResources(::GetCurrentProcess(), GR_USEROBJECTS);
    const DWORD gdi_before = ::GetGuiResources(::GetCurrentProcess(), GR_GDIOBJECTS);
    for (int iteration = 0; iteration < 40; ++iteration) {
        TreeView stress_tree;
        ListView stress_list;
        if (!stress_tree.Create(owner, {200 + iteration * 2}, {0.0_dip, 0.0_dip, 20.0_dip, 20.0_dip}) ||
            !stress_tree.AddItem({static_cast<std::uint64_t>(iteration + 1)}, L"item") ||
            !stress_list.Create(owner, {201 + iteration * 2}, {0.0_dip, 0.0_dip, 20.0_dip, 20.0_dip},
                                {.virtual_data = true}) ||
            !stress_list.SetVirtualModel(model))
            return 22;
    }
    const DWORD user_after = ::GetGuiResources(::GetCurrentProcess(), GR_USEROBJECTS);
    const DWORD gdi_after = ::GetGuiResources(::GetCurrentProcess(), GR_GDIOBJECTS);
    if (user_after > user_before + 8 || gdi_after > gdi_before + 8) return 23;

    state.virtual_list = nullptr;
    if (!tree.RemoveItem({20}) || tree.FindItem({20}) != nullptr ||
        !list.RemoveItem({20}) || list.FindItem({20}))
        return 24;
    virtual_list.Destroy();
    list.Destroy();
    tree.Destroy();
    ::DestroyWindow(owner);
    return EXIT_SUCCESS;
}
