#pragma once

#include <windows.h>
#include <commctrl.h>

#include <concepts>
#include <string_view>
#include <optional>

#include <mwtl/concepts.h>
#include <mwtl/controls.h>
#include <mwtl/tab_workspace.h>

namespace mwtl {

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
    bool Expand(HTREEITEM item, bool expanded = true) noexcept;
};

struct ListViewOptions {
    DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS;
    DWORD extended_style = WS_EX_CLIENTEDGE;
};

class ListView final : public NativeControl {
public:
    bool Create(HWND parent, ControlId id, RectDip bounds, ListViewOptions options = {});
    template <WindowLike Parent>
    bool Create(const Parent& parent, ControlId id, RectDip bounds, ListViewOptions options = {}) { return Create(parent.GetHwnd(), id, bounds, options); }
    int AddColumn(std::wstring_view text, int width_pixels, int index = -1);
    int AddItem(std::wstring_view text, int index = -1);
    bool SetSubItem(int item, int sub_item, std::wstring_view text);
    ListView& SetExtendedListStyle(DWORD style) noexcept;
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
    int GetSelection() const noexcept;
    std::optional<int> GetSelectedIndex() const noexcept {
        const int index = GetSelection();
        return index == CB_ERR ? std::nullopt : std::optional<int>{index};
    }
    bool SetSelection(int index) noexcept;
};

}  // namespace mwtl
