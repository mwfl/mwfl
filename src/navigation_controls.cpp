#include <mwtl/navigation_controls.h>

#include <algorithm>
#include <exception>
#include <limits>
#include <string>
#include <unordered_set>
#include <utility>

namespace mwtl {
namespace {

bool Initialize(DWORD classes) noexcept {
    INITCOMMONCONTROLSEX controls{sizeof(controls), classes};
    return ::InitCommonControlsEx(&controls) != FALSE;
}

struct TreeSortContext {
    const std::function<bool(TreeItemId, TreeItemId)>* less = nullptr;
    std::exception_ptr exception;
};

int CALLBACK CompareTreeItems(LPARAM left, LPARAM right, LPARAM context_value) noexcept {
    auto& context = *reinterpret_cast<TreeSortContext*>(context_value);
    try {
        const TreeItemId left_id{static_cast<std::uint64_t>(left)};
        const TreeItemId right_id{static_cast<std::uint64_t>(right)};
        if ((*context.less)(left_id, right_id)) return -1;
        if ((*context.less)(right_id, left_id)) return 1;
    } catch (...) {
        context.exception = std::current_exception();
    }
    return 0;
}

struct ListSortContext {
    const std::function<bool(ListItemId, ListItemId)>* less = nullptr;
    std::exception_ptr exception;
};

int CALLBACK CompareListItems(LPARAM left, LPARAM right, LPARAM context_value) noexcept {
    auto& context = *reinterpret_cast<ListSortContext*>(context_value);
    try {
        const ListItemId left_id{static_cast<std::uint64_t>(left)};
        const ListItemId right_id{static_cast<std::uint64_t>(right)};
        if ((*context.less)(left_id, right_id)) return -1;
        if ((*context.less)(right_id, left_id)) return 1;
    } catch (...) {
        context.exception = std::current_exception();
    }
    return 0;
}

}  // namespace

bool TreeView::Create(HWND parent, ControlId id, RectDip bounds, TreeViewOptions options) {
    next_id_ = 1;
    return Initialize(ICC_TREEVIEW_CLASSES) && CreateNative(WC_TREEVIEWW, parent, id, L"", bounds, options.style, options.extended_style);
}

HTREEITEM TreeView::AddItem(std::wstring_view text, HTREEITEM parent, HTREEITEM after) {
    return AddItem(GenerateId(), text, parent, after);
}

HTREEITEM TreeView::AddItem(TreeItemId id, std::wstring_view text, HTREEITEM parent,
                            HTREEITEM after) {
    if (!IsWindow()) {
        ::SetLastError(ERROR_INVALID_WINDOW_HANDLE);
        return nullptr;
    }
    if (!id || id.value > static_cast<std::uint64_t>((std::numeric_limits<INT_PTR>::max)()) ||
        FindItem(id) != nullptr) {
        ::SetLastError(ERROR_INVALID_PARAMETER);
        return nullptr;
    }
    std::wstring value{text};
    TVINSERTSTRUCTW item{};
    item.hParent = parent;
    item.hInsertAfter = after;
    item.item.mask = TVIF_TEXT | TVIF_PARAM;
    item.item.pszText = value.data();
    item.item.lParam = static_cast<LPARAM>(id.value);
    return reinterpret_cast<HTREEITEM>(::SendMessageW(GetHwnd(), TVM_INSERTITEMW, 0, reinterpret_cast<LPARAM>(&item)));
}

HTREEITEM TreeView::AddChild(TreeItemId id, std::wstring_view text, TreeItemId parent,
                             HTREEITEM after) {
    const HTREEITEM parent_item = FindItem(parent);
    if (parent_item == nullptr) {
        ::SetLastError(ERROR_NOT_FOUND);
        return nullptr;
    }
    return AddItem(id, text, parent_item, after);
}

TreeItemId TreeView::GenerateId() noexcept {
    while (next_id_ != 0) {
        const TreeItemId candidate{next_id_++};
        if (FindItem(candidate) == nullptr) return candidate;
    }
    return {};
}

std::optional<TreeItemId> TreeView::GetItemId(HTREEITEM item_handle) const noexcept {
    if (!IsWindow() || item_handle == nullptr) return std::nullopt;
    TVITEMW item{};
    item.mask = TVIF_PARAM;
    item.hItem = item_handle;
    if (TreeView_GetItem(GetHwnd(), &item) == FALSE || item.lParam == 0) return std::nullopt;
    return TreeItemId{static_cast<std::uint64_t>(item.lParam)};
}

HTREEITEM TreeView::FindItemRecursive(HTREEITEM item, TreeItemId id) const noexcept {
    while (item != nullptr) {
        if (GetItemId(item) == id) return item;
        if (const HTREEITEM child = TreeView_GetChild(GetHwnd(), item)) {
            if (const HTREEITEM found = FindItemRecursive(child, id)) return found;
        }
        item = TreeView_GetNextSibling(GetHwnd(), item);
    }
    return nullptr;
}

HTREEITEM TreeView::FindItem(TreeItemId id) const noexcept {
    return id && IsWindow() ? FindItemRecursive(TreeView_GetRoot(GetHwnd()), id) : nullptr;
}

std::optional<TreeItemId> TreeView::GetSelectedItemId() const noexcept {
    return IsWindow() ? GetItemId(TreeView_GetSelection(GetHwnd())) : std::nullopt;
}

bool TreeView::SetSelection(TreeItemId id) noexcept {
    if (!IsWindow() || !id) {
        ::SetLastError(!IsWindow() ? ERROR_INVALID_WINDOW_HANDLE : ERROR_INVALID_PARAMETER);
        return false;
    }
    const HTREEITEM item = FindItem(id);
    if (item == nullptr) {
        ::SetLastError(ERROR_NOT_FOUND);
        return false;
    }
    return TreeView_SelectItem(GetHwnd(), item) != FALSE;
}

bool TreeView::SetItemText(TreeItemId id, std::wstring_view text) {
    if (!IsWindow() || !id) {
        ::SetLastError(!IsWindow() ? ERROR_INVALID_WINDOW_HANDLE : ERROR_INVALID_PARAMETER);
        return false;
    }
    const HTREEITEM handle = FindItem(id);
    if (handle == nullptr) {
        ::SetLastError(ERROR_NOT_FOUND);
        return false;
    }
    std::wstring value{text};
    TVITEMW item{};
    item.mask = TVIF_TEXT;
    item.hItem = handle;
    item.pszText = value.data();
    return TreeView_SetItem(GetHwnd(), &item) != FALSE;
}

bool TreeView::RemoveItem(TreeItemId id) noexcept {
    if (!IsWindow() || !id) {
        ::SetLastError(!IsWindow() ? ERROR_INVALID_WINDOW_HANDLE : ERROR_INVALID_PARAMETER);
        return false;
    }
    const HTREEITEM item = FindItem(id);
    if (item == nullptr) {
        ::SetLastError(ERROR_NOT_FOUND);
        return false;
    }
    return TreeView_DeleteItem(GetHwnd(), item) != FALSE;
}

bool TreeView::SetStateImage(TreeItemId id, int image_index) noexcept {
    if (!IsWindow() || !id) {
        ::SetLastError(!IsWindow() ? ERROR_INVALID_WINDOW_HANDLE : ERROR_INVALID_PARAMETER);
        return false;
    }
    const HTREEITEM handle = FindItem(id);
    if (handle == nullptr || image_index < 0 || image_index > 15) {
        ::SetLastError(handle == nullptr ? ERROR_NOT_FOUND : ERROR_INVALID_PARAMETER);
        return false;
    }
    TVITEMW item{};
    item.mask = TVIF_STATE;
    item.hItem = handle;
    item.stateMask = TVIS_STATEIMAGEMASK;
    item.state = INDEXTOSTATEIMAGEMASK(image_index);
    return TreeView_SetItem(GetHwnd(), &item) != FALSE;
}

bool TreeView::BeginLabelEdit(TreeItemId id) noexcept {
    if (!IsWindow() || !id) {
        ::SetLastError(!IsWindow() ? ERROR_INVALID_WINDOW_HANDLE : ERROR_INVALID_PARAMETER);
        return false;
    }
    const HTREEITEM item = FindItem(id);
    if (item == nullptr) {
        ::SetLastError(ERROR_NOT_FOUND);
        return false;
    }
    return TreeView_EditLabel(GetHwnd(), item) != nullptr;
}

bool TreeView::EndLabelEdit(bool cancel) noexcept {
    if (!IsWindow()) {
        ::SetLastError(ERROR_INVALID_WINDOW_HANDLE);
        return false;
    }
    static_cast<void>(::SendMessageW(GetHwnd(), TVM_ENDEDITLABELNOW,
                                     cancel ? TRUE : FALSE, 0));
    return TreeView_GetEditControl(GetHwnd()) == nullptr;
}

bool TreeView::SortChildren(HTREEITEM parent,
                            const std::function<bool(TreeItemId, TreeItemId)>& less) {
    if (!IsWindow() || !less) {
        ::SetLastError(!IsWindow() ? ERROR_INVALID_WINDOW_HANDLE : ERROR_INVALID_PARAMETER);
        return false;
    }
    TreeSortContext context{.less = &less, .exception = {}};
    TVSORTCB sort{parent, CompareTreeItems, reinterpret_cast<LPARAM>(&context)};
    const bool sorted = TreeView_SortChildrenCB(GetHwnd(), &sort, FALSE) != FALSE;
    if (context.exception) std::rethrow_exception(context.exception);
    return sorted;
}

std::optional<TreeViewNotification> TreeView::DecodeNotification(const NMHDR& header) const {
    if (!IsWindow() || header.hwndFrom != GetHwnd()) return std::nullopt;
    TreeViewNotification result{};
    switch (header.code) {
        case TVN_SELCHANGEDW: {
            const auto& event = reinterpret_cast<const NMTREEVIEWW&>(header);
            result.kind = TreeViewNotificationKind::selection_changed;
            result.item = TreeItemId{static_cast<std::uint64_t>(event.itemNew.lParam)};
            result.previous_item = TreeItemId{static_cast<std::uint64_t>(event.itemOld.lParam)};
            result.action = event.action;
            return result;
        }
        case TVN_BEGINLABELEDITW:
        case TVN_ENDLABELEDITW: {
            const auto& event = reinterpret_cast<const NMTVDISPINFOW&>(header);
            result.kind = header.code == TVN_BEGINLABELEDITW
                              ? TreeViewNotificationKind::label_edit_begin
                              : TreeViewNotificationKind::label_edit_end;
            result.item = TreeItemId{static_cast<std::uint64_t>(event.item.lParam)};
            if (event.item.pszText != nullptr) result.label = event.item.pszText;
            return result;
        }
        case TVN_ITEMEXPANDEDW: {
            const auto& event = reinterpret_cast<const NMTREEVIEWW&>(header);
            result.kind = TreeViewNotificationKind::item_expanded;
            result.item = TreeItemId{static_cast<std::uint64_t>(event.itemNew.lParam)};
            result.action = event.action;
            return result;
        }
        case TVN_ITEMCHANGEDW: {
            const auto& event = reinterpret_cast<const NMTVITEMCHANGE&>(header);
            result.kind = TreeViewNotificationKind::state_changed;
            result.item = TreeItemId{static_cast<std::uint64_t>(event.lParam)};
            result.old_state = event.uStateOld;
            result.new_state = event.uStateNew;
            return result;
        }
        default:
            return std::nullopt;
    }
}

bool TreeView::Expand(HTREEITEM item, bool expanded) noexcept {
    return IsWindow() && ::SendMessageW(GetHwnd(), TVM_EXPAND, expanded ? TVE_EXPAND : TVE_COLLAPSE, reinterpret_cast<LPARAM>(item)) != FALSE;
}

bool TreeView::Expand(TreeItemId id, bool expanded) noexcept {
    const HTREEITEM item = FindItem(id);
    if (item == nullptr) {
        ::SetLastError(ERROR_NOT_FOUND);
        return false;
    }
    return Expand(item, expanded);
}

std::optional<std::size_t> VirtualListModel::FindRow(ListItemId id) const noexcept {
    if (!id) return std::nullopt;
    const std::size_t count = GetRowCount();
    for (std::size_t row = 0; row < count; ++row) {
        if (GetRowId(row) == id) return row;
    }
    return std::nullopt;
}

bool ListView::Create(HWND parent, ControlId id, RectDip bounds, ListViewOptions options) {
    model_.reset();
    virtual_exception_ = nullptr;
    next_id_ = 1;
    if (options.virtual_data) options.style |= LVS_OWNERDATA;
    return Initialize(ICC_LISTVIEW_CLASSES) && CreateNative(WC_LISTVIEWW, parent, id, L"", bounds, options.style, options.extended_style);
}

int ListView::AddColumn(std::wstring_view text, int width_pixels, int index) {
    if (!IsWindow()) return -1;
    std::wstring value{text};
    LVCOLUMNW column{};
    column.mask = LVCF_TEXT | LVCF_WIDTH;
    column.pszText = value.data();
    column.cx = width_pixels;
    const int position = index < 0 ? Header_GetItemCount(ListView_GetHeader(GetHwnd())) : index;
    return ListView_InsertColumn(GetHwnd(), position, &column);
}

int ListView::AddItem(std::wstring_view text, int index) {
    return AddItem(GenerateId(), text, index);
}

int ListView::AddItem(ListItemId id, std::wstring_view text, int index) {
    if (!IsWindow()) {
        ::SetLastError(ERROR_INVALID_WINDOW_HANDLE);
        return -1;
    }
    if (IsVirtual() || !id ||
        id.value > static_cast<std::uint64_t>((std::numeric_limits<INT_PTR>::max)()) ||
        FindItem(id)) {
        ::SetLastError(IsVirtual() ? ERROR_INVALID_STATE : ERROR_INVALID_PARAMETER);
        return -1;
    }
    std::wstring value{text};
    LVITEMW item{};
    item.mask = LVIF_TEXT | LVIF_PARAM;
    item.iItem = index < 0 ? ListView_GetItemCount(GetHwnd()) : index;
    item.pszText = value.data();
    item.lParam = static_cast<LPARAM>(id.value);
    return ListView_InsertItem(GetHwnd(), &item);
}

ListItemId ListView::GenerateId() noexcept {
    while (next_id_ != 0) {
        const ListItemId candidate{next_id_++};
        if (!FindItem(candidate)) return candidate;
    }
    return {};
}

bool ListView::SetSubItem(int item, int sub_item, std::wstring_view text) {
    if (!IsWindow() || IsVirtual()) {
        ::SetLastError(!IsWindow() ? ERROR_INVALID_WINDOW_HANDLE : ERROR_INVALID_STATE);
        return false;
    }
    std::wstring value{text};
    LVITEMW value_item{};
    value_item.iSubItem = sub_item;
    value_item.pszText = value.data();
    return ::SendMessageW(
        GetHwnd(), LVM_SETITEMTEXTW, item,
        reinterpret_cast<LPARAM>(&value_item)) != FALSE;
}

bool ListView::SetItemText(ListItemId id, int sub_item, std::wstring_view text) {
    const auto index = FindItem(id);
    if (!index) {
        ::SetLastError(ERROR_NOT_FOUND);
        return false;
    }
    return SetSubItem(*index, sub_item, text);
}

bool ListView::IsVirtual() const noexcept {
    return IsWindow() &&
           (static_cast<DWORD>(::GetWindowLongPtrW(GetHwnd(), GWL_STYLE)) & LVS_OWNERDATA) != 0;
}

std::optional<ListItemId> ListView::GetItemId(int index) const noexcept {
    if (!IsWindow() || index < 0 || index >= ListView_GetItemCount(GetHwnd())) return std::nullopt;
    if (IsVirtual()) {
        if (!model_ || static_cast<std::size_t>(index) >= model_->GetRowCount()) return std::nullopt;
        const ListItemId id = model_->GetRowId(static_cast<std::size_t>(index));
        return id ? std::optional<ListItemId>{id} : std::nullopt;
    }
    LVITEMW item{};
    item.mask = LVIF_PARAM;
    item.iItem = index;
    if (ListView_GetItem(GetHwnd(), &item) == FALSE || item.lParam == 0) return std::nullopt;
    return ListItemId{static_cast<std::uint64_t>(item.lParam)};
}

std::optional<int> ListView::FindItem(ListItemId id) const noexcept {
    if (!id || !IsWindow()) return std::nullopt;
    if (IsVirtual()) {
        if (!model_) return std::nullopt;
        const auto row = model_->FindRow(id);
        if (!row || *row > static_cast<std::size_t>((std::numeric_limits<int>::max)()))
            return std::nullopt;
        return static_cast<int>(*row);
    }
    LVFINDINFOW find{};
    find.flags = LVFI_PARAM;
    find.lParam = static_cast<LPARAM>(id.value);
    const int index = ListView_FindItem(GetHwnd(), -1, &find);
    return index < 0 ? std::nullopt : std::optional<int>{index};
}

std::vector<ListItemId> ListView::GetSelectedItemIds() const {
    std::vector<ListItemId> selected;
    if (!IsWindow()) return selected;
    int index = -1;
    while ((index = ListView_GetNextItem(GetHwnd(), index, LVNI_SELECTED)) >= 0) {
        if (const auto id = GetItemId(index)) selected.push_back(*id);
    }
    return selected;
}

bool ListView::SetSelected(ListItemId id, bool selected, bool clear_existing) noexcept {
    if (!IsWindow() || !id) {
        ::SetLastError(!IsWindow() ? ERROR_INVALID_WINDOW_HANDLE : ERROR_INVALID_PARAMETER);
        return false;
    }
    const auto index = FindItem(id);
    if (!index) {
        ::SetLastError(ERROR_NOT_FOUND);
        return false;
    }
    if (clear_existing)
        ListView_SetItemState(GetHwnd(), -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
    ListView_SetItemState(GetHwnd(), *index, selected ? LVIS_SELECTED : 0, LVIS_SELECTED);
    const bool native_selected =
        (ListView_GetItemState(GetHwnd(), *index, LVIS_SELECTED) & LVIS_SELECTED) != 0;
    return native_selected == selected;
}

bool ListView::RemoveItem(ListItemId id) noexcept {
    if (!IsWindow() || !id) {
        ::SetLastError(!IsWindow() ? ERROR_INVALID_WINDOW_HANDLE : ERROR_INVALID_PARAMETER);
        return false;
    }
    if (IsVirtual()) {
        ::SetLastError(ERROR_INVALID_STATE);
        return false;
    }
    const auto index = FindItem(id);
    if (!index) {
        ::SetLastError(ERROR_NOT_FOUND);
        return false;
    }
    return ListView_DeleteItem(GetHwnd(), *index) != FALSE;
}

bool ListView::SetStateImage(ListItemId id, int image_index) noexcept {
    if (!IsWindow() || !id) {
        ::SetLastError(!IsWindow() ? ERROR_INVALID_WINDOW_HANDLE : ERROR_INVALID_PARAMETER);
        return false;
    }
    if (IsVirtual()) {
        ::SetLastError(ERROR_INVALID_STATE);
        return false;
    }
    const auto index = FindItem(id);
    if (!index || image_index < 0 || image_index > 15) {
        ::SetLastError(!index ? ERROR_NOT_FOUND : ERROR_INVALID_PARAMETER);
        return false;
    }
    ListView_SetItemState(GetHwnd(), *index, INDEXTOSTATEIMAGEMASK(image_index),
                          LVIS_STATEIMAGEMASK);
    return (ListView_GetItemState(GetHwnd(), *index, LVIS_STATEIMAGEMASK) &
            LVIS_STATEIMAGEMASK) == static_cast<UINT>(INDEXTOSTATEIMAGEMASK(image_index));
}

bool ListView::BeginLabelEdit(ListItemId id) noexcept {
    if (!IsWindow() || !id) {
        ::SetLastError(!IsWindow() ? ERROR_INVALID_WINDOW_HANDLE : ERROR_INVALID_PARAMETER);
        return false;
    }
    const auto index = FindItem(id);
    if (!index) {
        ::SetLastError(ERROR_NOT_FOUND);
        return false;
    }
    return ListView_EditLabel(GetHwnd(), *index) != nullptr;
}

bool ListView::CancelLabelEdit() noexcept {
    if (!IsWindow()) {
        ::SetLastError(ERROR_INVALID_WINDOW_HANDLE);
        return false;
    }
    static_cast<void>(::SendMessageW(GetHwnd(), LVM_CANCELEDITLABEL, 0, 0));
    return ListView_GetEditControl(GetHwnd()) == nullptr;
}

bool ListView::SortItems(const std::function<bool(ListItemId, ListItemId)>& less) {
    if (!IsWindow() || IsVirtual() || !less) {
        ::SetLastError(!IsWindow() ? ERROR_INVALID_WINDOW_HANDLE
                                   : IsVirtual() ? ERROR_INVALID_STATE : ERROR_INVALID_PARAMETER);
        return false;
    }
    ListSortContext context{.less = &less, .exception = {}};
    const bool sorted = ListView_SortItems(GetHwnd(), CompareListItems,
                                           reinterpret_cast<LPARAM>(&context)) != FALSE;
    if (context.exception) std::rethrow_exception(context.exception);
    return sorted;
}

bool ListView::SetVirtualModel(std::shared_ptr<const VirtualListModel> model) {
    if (!IsWindow() || !IsVirtual() || !model) {
        ::SetLastError(!IsWindow() ? ERROR_INVALID_WINDOW_HANDLE
                                   : !IsVirtual() ? ERROR_INVALID_STATE : ERROR_INVALID_PARAMETER);
        return false;
    }
    const std::vector<ListItemId> selected = model_ ? GetSelectedItemIds()
                                                     : std::vector<ListItemId>{};
    const std::size_t count = model->GetRowCount();
    if (count > static_cast<std::size_t>((std::numeric_limits<int>::max)())) {
        ::SetLastError(ERROR_ARITHMETIC_OVERFLOW);
        return false;
    }
    std::unordered_set<std::uint64_t> ids;
    ids.reserve(count);
    for (std::size_t row = 0; row < count; ++row) {
        const ListItemId id = model->GetRowId(row);
        const int state_image = model->GetStateImageIndex(row);
        if (!id || !ids.insert(id.value).second || state_image < 0 || state_image > 15) {
            ::SetLastError(ERROR_INVALID_DATA);
            return false;
        }
    }
    auto previous = std::move(model_);
    model_ = std::move(model);
    if (ListView_SetItemCountEx(GetHwnd(), static_cast<int>(count),
                                LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL) == FALSE) {
        model_ = std::move(previous);
        return false;
    }
    ListView_SetItemState(GetHwnd(), -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
    for (const ListItemId id : selected) {
        if (FindItem(id)) static_cast<void>(SetSelected(id));
    }
    return true;
}

bool ListView::RefreshVirtualModel() {
    if (!model_) {
        ::SetLastError(ERROR_INVALID_STATE);
        return false;
    }
    return SetVirtualModel(model_);
}

bool ListView::UpdateVirtualModel(const std::function<void()>& update) {
    if (!model_ || !update) {
        ::SetLastError(ERROR_INVALID_STATE);
        return false;
    }
    const std::vector<ListItemId> selected = GetSelectedItemIds();
    update();
    if (!SetVirtualModel(model_)) return false;
    ListView_SetItemState(GetHwnd(), -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
    for (const ListItemId id : selected) {
        if (FindItem(id)) static_cast<void>(SetSelected(id));
    }
    return true;
}

bool ListView::HandleNotification(const NMHDR& header, LRESULT& result) const noexcept {
    if (!IsVirtual() || !model_ || header.hwndFrom != GetHwnd() ||
        header.code != LVN_GETDISPINFOW)
        return false;
    auto& info = const_cast<NMLVDISPINFOW&>(reinterpret_cast<const NMLVDISPINFOW&>(header));
    if (info.item.iItem < 0 ||
        static_cast<std::size_t>(info.item.iItem) >= model_->GetRowCount()) {
        result = 0;
        return true;
    }
    try {
        const std::size_t row = static_cast<std::size_t>(info.item.iItem);
        if ((info.item.mask & LVIF_TEXT) != 0 && info.item.pszText != nullptr &&
            info.item.cchTextMax > 0) {
            const std::wstring text = model_->GetCellText(row, info.item.iSubItem);
            static_cast<void>(wcsncpy_s(info.item.pszText,
                                        static_cast<std::size_t>(info.item.cchTextMax),
                                        text.c_str(), _TRUNCATE));
        }
        if ((info.item.mask & LVIF_IMAGE) != 0)
            info.item.iImage = model_->GetImageIndex(row, info.item.iSubItem);
        if ((info.item.mask & LVIF_STATE) != 0) {
            info.item.stateMask |= LVIS_STATEIMAGEMASK;
            info.item.state = (info.item.state & ~LVIS_STATEIMAGEMASK) |
                              INDEXTOSTATEIMAGEMASK(model_->GetStateImageIndex(row));
        }
    } catch (...) {
        if (!virtual_exception_) virtual_exception_ = std::current_exception();
        if ((info.item.mask & LVIF_TEXT) != 0 && info.item.pszText != nullptr &&
            info.item.cchTextMax > 0)
            info.item.pszText[0] = L'\0';
        ::SetLastError(ERROR_UNHANDLED_EXCEPTION);
    }
    result = 0;
    return true;
}

std::exception_ptr ListView::TakeVirtualException() noexcept {
    return std::exchange(virtual_exception_, nullptr);
}

std::optional<ListViewNotification> ListView::DecodeNotification(const NMHDR& header) const {
    if (!IsWindow() || header.hwndFrom != GetHwnd()) return std::nullopt;
    ListViewNotification result{};
    switch (header.code) {
        case LVN_ITEMCHANGED: {
            const auto& event = reinterpret_cast<const NMLISTVIEW&>(header);
            if ((event.uChanged & LVIF_STATE) == 0) return std::nullopt;
            result.kind = ListViewNotificationKind::selection_changed;
            result.row = event.iItem;
            result.item = GetItemId(event.iItem).value_or(ListItemId{});
            result.old_state = event.uOldState;
            result.new_state = event.uNewState;
            return result;
        }
        case LVN_BEGINLABELEDITW:
        case LVN_ENDLABELEDITW: {
            const auto& event = reinterpret_cast<const NMLVDISPINFOW&>(header);
            result.kind = header.code == LVN_BEGINLABELEDITW
                              ? ListViewNotificationKind::label_edit_begin
                              : ListViewNotificationKind::label_edit_end;
            result.row = event.item.iItem;
            result.item = GetItemId(event.item.iItem).value_or(ListItemId{});
            if (event.item.pszText != nullptr) result.label = event.item.pszText;
            return result;
        }
        case LVN_COLUMNCLICK: {
            const auto& event = reinterpret_cast<const NMLISTVIEW&>(header);
            result.kind = ListViewNotificationKind::column_clicked;
            result.column = event.iSubItem;
            return result;
        }
        case LVN_ITEMACTIVATE: {
            const auto& event = reinterpret_cast<const NMITEMACTIVATE&>(header);
            result.kind = ListViewNotificationKind::activated;
            result.row = event.iItem;
            result.column = event.iSubItem;
            result.item = GetItemId(event.iItem).value_or(ListItemId{});
            return result;
        }
        case LVN_ODCACHEHINT: {
            const auto& event = reinterpret_cast<const NMLVCACHEHINT&>(header);
            result.kind = ListViewNotificationKind::virtual_cache_hint;
            result.cache_first = event.iFrom;
            result.cache_last = event.iTo;
            return result;
        }
        default:
            return std::nullopt;
    }
}

ListView& ListView::SetExtendedListStyle(DWORD style) noexcept {
    if (IsWindow()) ListView_SetExtendedListViewStyle(GetHwnd(), style);
    return *this;
}

bool Header::Create(HWND parent, ControlId id, RectDip bounds, HeaderOptions options) {
    return Initialize(ICC_LISTVIEW_CLASSES) && CreateNative(WC_HEADERW, parent, id, L"", bounds, options.style, options.extended_style);
}

int Header::AddColumn(std::wstring_view text, int width_pixels, int index) {
    if (!IsWindow()) return -1;
    std::wstring value{text};
    HDITEMW item{};
    item.mask = HDI_TEXT | HDI_WIDTH;
    item.pszText = value.data();
    item.cxy = width_pixels;
    const int position = index < 0 ? Header_GetItemCount(GetHwnd()) : index;
    return Header_InsertItem(GetHwnd(), position, &item);
}

bool TabControl::Create(HWND parent, ControlId id, RectDip bounds, TabControlOptions options) {
    return Initialize(ICC_TAB_CLASSES) && CreateNative(WC_TABCONTROLW, parent, id, L"", bounds, options.style, options.extended_style);
}

int TabControl::AddTab(std::wstring_view text, int index) {
    return AddTab({}, text, index);
}

int TabControl::AddTab(TabId id, std::wstring_view text, int index) {
    if (!IsWindow()) return -1;
    std::wstring value{text};
    TCITEMW item{};
    item.mask = TCIF_TEXT;
    if (id) {
        item.mask |= TCIF_PARAM;
        item.lParam = static_cast<LPARAM>(id.value);
    }
    item.pszText = value.data();
    const int position = index < 0 ? TabCtrl_GetItemCount(GetHwnd()) : index;
    return TabCtrl_InsertItem(GetHwnd(), position, &item);
}

int TabControl::GetSelection() const noexcept {
    return IsWindow() ? TabCtrl_GetCurSel(GetHwnd()) : -1;
}
bool TabControl::SetSelection(int index) noexcept {
    if (!IsWindow() || index < 0 || index >= TabCtrl_GetItemCount(GetHwnd())) return false;
    static_cast<void>(TabCtrl_SetCurSel(GetHwnd(), index));
    return GetSelection() == index;
}

std::optional<TabId> TabControl::GetTabId(int index) const noexcept {
    if (!IsWindow() || index < 0 || index >= TabCtrl_GetItemCount(GetHwnd())) return std::nullopt;
    TCITEMW item{};
    item.mask = TCIF_PARAM;
    if (TabCtrl_GetItem(GetHwnd(), index, &item) == FALSE || item.lParam == 0) return std::nullopt;
    return TabId{static_cast<std::uint64_t>(item.lParam)};
}

std::optional<TabId> TabControl::GetSelectedTabId() const noexcept {
    return GetTabId(GetSelection());
}

bool TabControl::SetSelection(TabId id) noexcept {
    if (!id || !IsWindow()) return false;
    const int count = TabCtrl_GetItemCount(GetHwnd());
    for (int index = 0; index < count; ++index) {
        if (GetTabId(index) == id) return SetSelection(index);
    }
    return false;
}

bool TabControl::RemoveTab(TabId id) noexcept {
    if (!id || !IsWindow()) return false;
    const auto selected = GetSelectedTabId();
    const int count = TabCtrl_GetItemCount(GetHwnd());
    for (int index = 0; index < count; ++index) {
        if (GetTabId(index) != id) continue;
        if (TabCtrl_DeleteItem(GetHwnd(), index) == FALSE) return false;
        const int remaining = TabCtrl_GetItemCount(GetHwnd());
        if (remaining == 0 || !selected) return true;
        if (*selected != id) return SetSelection(*selected);
        return SetSelection((std::min)(index, remaining - 1));
    }
    return false;
}

bool TabControl::Synchronize(const TabWorkspaceModel& model) {
    if (!IsWindow() || TabCtrl_DeleteAllItems(GetHwnd()) == FALSE) return false;
    for (const auto& tab : model.GetTabs()) {
        if (AddTab(tab.id, tab.title) < 0) return false;
    }
    const auto selected = model.GetSelectedId();
    return !selected || SetSelection(*selected);
}

bool ComboBoxEx::Create(HWND parent, ControlId id, RectDip bounds, ComboBoxExOptions options) {
    return Initialize(ICC_USEREX_CLASSES) && CreateNative(WC_COMBOBOXEXW, parent, id, L"", bounds, options.style, options.extended_style);
}

int ComboBoxEx::AddItem(std::wstring_view text, int index) {
    if (!IsWindow()) return CB_ERR;
    std::wstring value{text};
    COMBOBOXEXITEMW item{};
    item.mask = CBEIF_TEXT;
    item.iItem = index < 0 ? -1 : index;
    item.pszText = value.data();
    return static_cast<int>(::SendMessageW(GetHwnd(), CBEM_INSERTITEMW, 0, reinterpret_cast<LPARAM>(&item)));
}

int ComboBoxEx::GetItemCount() const noexcept {
    return IsWindow() ? static_cast<int>(::SendMessageW(GetHwnd(), CB_GETCOUNT, 0, 0)) : CB_ERR;
}

std::optional<std::wstring> ComboBoxEx::GetItemText(int index) const {
    if (!IsWindow()) {
        ::SetLastError(ERROR_INVALID_WINDOW_HANDLE);
        return std::nullopt;
    }
    const int count = GetItemCount();
    if (index < 0 || count == CB_ERR || index >= count) {
        ::SetLastError(ERROR_INVALID_INDEX);
        return std::nullopt;
    }
    const LRESULT length = ::SendMessageW(GetHwnd(), CB_GETLBTEXTLEN, index, 0);
    if (length == CB_ERR) return std::nullopt;
    std::wstring value(static_cast<std::size_t>(length) + 1, L'\0');
    const LRESULT copied = ::SendMessageW(GetHwnd(), CB_GETLBTEXT, index,
                                           reinterpret_cast<LPARAM>(value.data()));
    if (copied == CB_ERR) return std::nullopt;
    value.resize(static_cast<std::size_t>(copied));
    return value;
}

bool ComboBoxEx::RemoveItem(int index) noexcept {
    if (!IsWindow()) {
        ::SetLastError(ERROR_INVALID_WINDOW_HANDLE);
        return false;
    }
    const int count = GetItemCount();
    if (index < 0 || count == CB_ERR || index >= count) {
        ::SetLastError(ERROR_INVALID_INDEX);
        return false;
    }
    return ::SendMessageW(GetHwnd(), CBEM_DELETEITEM, index, 0) != CB_ERR;
}

bool ComboBoxEx::ClearItems() noexcept {
    if (!IsWindow()) {
        ::SetLastError(ERROR_INVALID_WINDOW_HANDLE);
        return false;
    }
    ::SendMessageW(GetHwnd(), CB_RESETCONTENT, 0, 0);
    return true;
}

int ComboBoxEx::GetSelection() const noexcept { return IsWindow() ? static_cast<int>(::SendMessageW(GetHwnd(), CB_GETCURSEL, 0, 0)) : CB_ERR; }
bool ComboBoxEx::SetSelection(int index) noexcept { return IsWindow() && ::SendMessageW(GetHwnd(), CB_SETCURSEL, index, 0) != CB_ERR; }

}  // namespace mwtl
