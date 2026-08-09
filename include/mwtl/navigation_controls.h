#pragma once

#include <windows.h>
#include <commctrl.h>

#include <concepts>
#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <mwtl/concepts.h>
#include <mwtl/controls.h>
#include <mwtl/tab_workspace.h>

namespace mwtl {

struct TreeItemId {
    std::uint64_t value = 0;
    constexpr explicit operator bool() const noexcept { return value != 0; }
    friend constexpr bool operator==(TreeItemId, TreeItemId) noexcept = default;
};

enum class TreeViewNotificationKind {
    selection_changed,
    label_edit_begin,
    label_edit_end,
    item_expanded,
    state_changed,
};

struct TreeViewNotification {
    TreeViewNotificationKind kind{};
    TreeItemId item{};
    TreeItemId previous_item{};
    std::optional<std::wstring> label;
    UINT action = 0;
    UINT old_state = 0;
    UINT new_state = 0;
};

struct TreeViewOptions {
    DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER |
        TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | TVS_SHOWSELALWAYS;
    DWORD extended_style = WS_EX_CLIENTEDGE;
};

class TreeView final : public NativeControl {
public:
    bool Create(HWND parent, ControlId id, RectDip bounds, TreeViewOptions options = {});
    template <WindowLike Parent>
    bool Create(const Parent& parent, ControlId id, RectDip bounds, TreeViewOptions options = {}) { return Create(parent.GetHwnd(), id, bounds, options); }
    HTREEITEM AddItem(std::wstring_view text, HTREEITEM parent = TVI_ROOT, HTREEITEM after = TVI_LAST);
    HTREEITEM AddItem(TreeItemId id, std::wstring_view text,
                      HTREEITEM parent = TVI_ROOT, HTREEITEM after = TVI_LAST);
    HTREEITEM AddChild(TreeItemId id, std::wstring_view text, TreeItemId parent,
                       HTREEITEM after = TVI_LAST);
    std::optional<TreeItemId> GetItemId(HTREEITEM item) const noexcept;
    HTREEITEM FindItem(TreeItemId id) const noexcept;
    std::optional<TreeItemId> GetSelectedItemId() const noexcept;
    bool SetSelection(TreeItemId id) noexcept;
    bool SetItemText(TreeItemId id, std::wstring_view text);
    bool RemoveItem(TreeItemId id) noexcept;
    bool SetStateImage(TreeItemId id, int image_index) noexcept;
    bool BeginLabelEdit(TreeItemId id) noexcept;
    bool EndLabelEdit(bool cancel) noexcept;
    bool SortChildren(HTREEITEM parent,
                      const std::function<bool(TreeItemId, TreeItemId)>& less);
    std::optional<TreeViewNotification> DecodeNotification(const NMHDR& header) const;
    bool Expand(HTREEITEM item, bool expanded = true) noexcept;
    bool Expand(TreeItemId id, bool expanded = true) noexcept;

private:
    TreeItemId GenerateId() noexcept;
    HTREEITEM FindItemRecursive(HTREEITEM item, TreeItemId id) const noexcept;
    std::uint64_t next_id_ = 1;
};

struct ListItemId {
    std::uint64_t value = 0;
    constexpr explicit operator bool() const noexcept { return value != 0; }
    friend constexpr bool operator==(ListItemId, ListItemId) noexcept = default;
};

class VirtualListModel {
public:
    // Shared, application-authoritative, UI-thread model for LVS_OWNERDATA.
    // No application pointer is stored in a native item.
    virtual ~VirtualListModel() = default;
    virtual std::size_t GetRowCount() const noexcept = 0;
    virtual ListItemId GetRowId(std::size_t row) const noexcept = 0;
    virtual std::wstring GetCellText(std::size_t row, int column) const = 0;
    virtual int GetImageIndex(std::size_t, int) const noexcept { return -1; }
    virtual int GetStateImageIndex(std::size_t) const noexcept { return 0; }
    virtual std::optional<std::size_t> FindRow(ListItemId id) const noexcept;
};

enum class ListViewNotificationKind {
    selection_changed,
    label_edit_begin,
    label_edit_end,
    column_clicked,
    activated,
    virtual_cache_hint,
};

struct ListViewNotification {
    ListViewNotificationKind kind{};
    ListItemId item{};
    int row = -1;
    int column = -1;
    std::optional<std::wstring> label;
    UINT old_state = 0;
    UINT new_state = 0;
    int cache_first = -1;
    int cache_last = -1;
};

struct ListViewOptions {
    DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | LVS_REPORT | LVS_SHOWSELALWAYS;
    DWORD extended_style = WS_EX_CLIENTEDGE;
    bool virtual_data = false;
};

class ListView final : public NativeControl {
public:
    // Owns its child HWND and a shared virtual model when present. Item IDs are
    // numeric native parameters; GetHwnd remains a borrowed escape hatch.
    bool Create(HWND parent, ControlId id, RectDip bounds, ListViewOptions options = {});
    template <WindowLike Parent>
    bool Create(const Parent& parent, ControlId id, RectDip bounds, ListViewOptions options = {}) { return Create(parent.GetHwnd(), id, bounds, options); }
    int AddColumn(std::wstring_view text, int width_pixels, int index = -1);
    int AddItem(std::wstring_view text, int index = -1);
    int AddItem(ListItemId id, std::wstring_view text, int index = -1);
    bool SetSubItem(int item, int sub_item, std::wstring_view text);
    bool SetItemText(ListItemId id, int sub_item, std::wstring_view text);
    std::optional<ListItemId> GetItemId(int index) const noexcept;
    std::optional<int> FindItem(ListItemId id) const noexcept;
    std::vector<ListItemId> GetSelectedItemIds() const;
    bool SetSelected(ListItemId id, bool selected = true,
                     bool clear_existing = false) noexcept;
    bool RemoveItem(ListItemId id) noexcept;
    bool SetStateImage(ListItemId id, int image_index) noexcept;
    bool BeginLabelEdit(ListItemId id) noexcept;
    bool CancelLabelEdit() noexcept;
    bool SortItems(const std::function<bool(ListItemId, ListItemId)>& less);
    bool SetVirtualModel(std::shared_ptr<const VirtualListModel> model);
    std::shared_ptr<const VirtualListModel> GetVirtualModel() const noexcept { return model_; }
    bool RefreshVirtualModel();
    bool UpdateVirtualModel(const std::function<void()>& update);
    bool HandleNotification(const NMHDR& header, LRESULT& result) const noexcept;
    std::exception_ptr TakeVirtualException() noexcept;
    std::optional<ListViewNotification> DecodeNotification(const NMHDR& header) const;
    bool IsVirtual() const noexcept;
    ListView& SetExtendedListStyle(DWORD style) noexcept;

private:
    ListItemId GenerateId() noexcept;
    std::uint64_t next_id_ = 1;
    std::shared_ptr<const VirtualListModel> model_;
    mutable std::exception_ptr virtual_exception_;
};

struct HeaderOptions {
    DWORD style = WS_CHILD | WS_VISIBLE | HDS_BUTTONS | HDS_HORZ;
    DWORD extended_style = 0;
};

class Header final : public NativeControl {
public:
    bool Create(HWND parent, ControlId id, RectDip bounds, HeaderOptions options = {});
    template <WindowLike Parent>
    bool Create(const Parent& parent, ControlId id, RectDip bounds, HeaderOptions options = {}) { return Create(parent.GetHwnd(), id, bounds, options); }
    int AddColumn(std::wstring_view text, int width_pixels, int index = -1);
};

struct TabControlOptions {
    DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | TCS_TABS;
    DWORD extended_style = 0;
};

class TabControl final : public NativeControl {
public:
    bool Create(HWND parent, ControlId id, RectDip bounds, TabControlOptions options = {});
    template <WindowLike Parent>
    bool Create(const Parent& parent, ControlId id, RectDip bounds, TabControlOptions options = {}) { return Create(parent.GetHwnd(), id, bounds, options); }
    int AddTab(std::wstring_view text, int index = -1);
    int AddTab(TabId id, std::wstring_view text, int index = -1);
    int GetSelection() const noexcept;
    std::optional<int> GetSelectedIndex() const noexcept {
        const int index = GetSelection();
        return index < 0 ? std::nullopt : std::optional<int>{index};
    }
    bool SetSelection(int index) noexcept;
    std::optional<TabId> GetTabId(int index) const noexcept;
    std::optional<TabId> GetSelectedTabId() const noexcept;
    bool SetSelection(TabId id) noexcept;
    bool RemoveTab(TabId id) noexcept;
    bool Synchronize(const TabWorkspaceModel& model);
};

struct ComboBoxExOptions {
    DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST;
    DWORD extended_style = 0;
};

class ComboBoxEx final : public NativeControl {
public:
    bool Create(HWND parent, ControlId id, RectDip bounds, ComboBoxExOptions options = {});
    template <WindowLike Parent>
    bool Create(const Parent& parent, ControlId id, RectDip bounds, ComboBoxExOptions options = {}) { return Create(parent.GetHwnd(), id, bounds, options); }
    int AddItem(std::wstring_view text, int index = -1);
    int GetItemCount() const noexcept;
    std::optional<std::wstring> GetItemText(int index) const;
    bool RemoveItem(int index) noexcept;
    bool ClearItems() noexcept;
    int GetSelection() const noexcept;
    std::optional<int> GetSelectedIndex() const noexcept {
        const int index = GetSelection();
        return index == CB_ERR ? std::nullopt : std::optional<int>{index};
    }
    bool SetSelection(int index) noexcept;
};

}  // namespace mwtl
