#include <mwtl/navigation_controls.h>

#include <algorithm>
#include <string>

namespace mwtl {
namespace {

bool Initialize(DWORD classes) noexcept {
    INITCOMMONCONTROLSEX controls{sizeof(controls), classes};
    return ::InitCommonControlsEx(&controls) != FALSE;
}

}  // namespace

bool TreeView::Create(HWND parent, ControlId id, RectDip bounds, TreeViewOptions options) {
    return Initialize(ICC_TREEVIEW_CLASSES) && CreateNative(WC_TREEVIEWW, parent, id, L"", bounds, options.style, options.extended_style);
}

HTREEITEM TreeView::AddItem(std::wstring_view text, HTREEITEM parent, HTREEITEM after) {
    if (!IsWindow()) return nullptr;
    std::wstring value{text};
    TVINSERTSTRUCTW item{};
    item.hParent = parent;
    item.hInsertAfter = after;
    item.item.mask = TVIF_TEXT;
    item.item.pszText = value.data();
    return reinterpret_cast<HTREEITEM>(::SendMessageW(GetHwnd(), TVM_INSERTITEMW, 0, reinterpret_cast<LPARAM>(&item)));
}

bool TreeView::Expand(HTREEITEM item, bool expanded) noexcept {
    return IsWindow() && ::SendMessageW(GetHwnd(), TVM_EXPAND, expanded ? TVE_EXPAND : TVE_COLLAPSE, reinterpret_cast<LPARAM>(item)) != FALSE;
}

bool ListView::Create(HWND parent, ControlId id, RectDip bounds, ListViewOptions options) {
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
    if (!IsWindow()) return -1;
    std::wstring value{text};
    LVITEMW item{};
    item.mask = LVIF_TEXT;
    item.iItem = index < 0 ? ListView_GetItemCount(GetHwnd()) : index;
    item.pszText = value.data();
    return ListView_InsertItem(GetHwnd(), &item);
}

bool ListView::SetSubItem(int item, int sub_item, std::wstring_view text) {
    if (!IsWindow()) return false;
    std::wstring value{text};
    LVITEMW value_item{};
    value_item.iSubItem = sub_item;
    value_item.pszText = value.data();
    return ::SendMessageW(
        GetHwnd(), LVM_SETITEMTEXTW, item,
        reinterpret_cast<LPARAM>(&value_item)) != FALSE;
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
