#pragma once

#include <windows.h>
#include <commctrl.h>

#include <concepts>
#include <string>
#include <string_view>
#include <optional>

#include <mwtl/concepts.h>
#include <mwtl/dpi.h>
#include <mwtl/events.h>

namespace mwtl {

SizeDip MeasureNativeControl(HWND window, DpiContext dpi);

class NativeControl {
public:
    NativeControl() noexcept = default;
    ~NativeControl() noexcept;

    NativeControl(const NativeControl&) = delete;
    NativeControl& operator=(const NativeControl&) = delete;
    NativeControl(NativeControl&& other) noexcept;
    NativeControl& operator=(NativeControl&& other) noexcept;

    HWND GetHwnd() const noexcept {
        return IsWindow() ? window_ : nullptr;
    }
    bool IsWindow() const noexcept;
    bool IsOwnerThread() const noexcept;
    ControlId GetId() const noexcept { return id_; }
    SizeDip GetPreferredSize(DpiContext dpi) const;

    bool SetText(std::wstring_view text);
    std::wstring GetText() const;
    bool SetBounds(RectDip bounds) noexcept;
    NativeControl& SetEnabled(bool enabled) noexcept;
    NativeControl& SetVisible(bool visible) noexcept;
    NativeControl& Focus() noexcept;
    void Destroy() noexcept;

protected:
    bool CreateNative(
        const wchar_t* class_name,
        HWND parent,
        ControlId id,
        std::wstring_view text,
        RectDip bounds,
        DWORD style,
        DWORD extended_style = 0);

private:
    HWND window_ = nullptr;  // Owned child HWND while valid.
    HWND parent_ = nullptr;  // Non-owning identity check.
    ControlId id_{};
    DWORD owner_thread_id_ = 0;
};

struct LabelOptions {
    DWORD style = WS_CHILD | WS_VISIBLE | SS_LEFT;
    DWORD extended_style = 0;
};

class Label final : public NativeControl {
public:
    bool Create(
        HWND parent,
        ControlId id,
        std::wstring_view text,
        RectDip bounds,
        LabelOptions options = {});

    template <WindowLike Parent>
    bool Create(
        const Parent& parent,
        ControlId id,
        std::wstring_view text,
        RectDip bounds,
        LabelOptions options = {}) {
        return Create(parent.GetHwnd(), id, text, bounds, options);
    }
};

struct ButtonOptions {
    DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON;
    DWORD extended_style = 0;
};

class Button final : public NativeControl {
public:
    bool Create(
        HWND parent,
        ControlId id,
        std::wstring_view text,
        RectDip bounds,
        ButtonOptions options = {});
    template <WindowLike Parent>
    bool Create(
        const Parent& parent,
        ControlId id,
        std::wstring_view text,
        RectDip bounds,
        ButtonOptions options = {}) {
        return Create(parent.GetHwnd(), id, text, bounds, options);
    }
    Button& Click() noexcept;
};

struct TextBoxOptions {
    DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL;
    DWORD extended_style = WS_EX_CLIENTEDGE;
};

class TextBox final : public NativeControl {
public:
    bool Create(
        HWND parent,
        ControlId id,
        std::wstring_view text,
        RectDip bounds,
        TextBoxOptions options = {});
    template <WindowLike Parent>
    bool Create(
        const Parent& parent,
        ControlId id,
        std::wstring_view text,
        RectDip bounds,
        TextBoxOptions options = {}) {
        return Create(parent.GetHwnd(), id, text, bounds, options);
    }
    TextBox& SelectAll() noexcept;
    TextBox& SetReadOnly(bool read_only) noexcept;
};

struct CheckBoxOptions {
    DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX;
    DWORD extended_style = 0;
};

class CheckBox final : public NativeControl {
public:
    bool Create(
        HWND parent,
        ControlId id,
        std::wstring_view text,
        RectDip bounds,
        CheckBoxOptions options = {});
    template <WindowLike Parent>
    bool Create(
        const Parent& parent,
        ControlId id,
        std::wstring_view text,
        RectDip bounds,
        CheckBoxOptions options = {}) {
        return Create(parent.GetHwnd(), id, text, bounds, options);
    }
    bool IsChecked() const noexcept;
    CheckBox& SetChecked(bool checked) noexcept;
};

struct RadioButtonOptions {
    DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTORADIOBUTTON;
    DWORD extended_style = 0;
};

class RadioButton final : public NativeControl {
public:
    bool Create(HWND parent, ControlId id, std::wstring_view text, RectDip bounds, RadioButtonOptions options = {});
    template <WindowLike Parent>
    bool Create(const Parent& parent, ControlId id, std::wstring_view text, RectDip bounds, RadioButtonOptions options = {}) {
        return Create(parent.GetHwnd(), id, text, bounds, options);
    }
    bool IsChecked() const noexcept;
    RadioButton& SetChecked(bool checked) noexcept;
};

struct GroupBoxOptions {
    DWORD style = WS_CHILD | WS_VISIBLE | BS_GROUPBOX;
    DWORD extended_style = 0;
};

class GroupBox final : public NativeControl {
public:
    bool Create(HWND parent, ControlId id, std::wstring_view text, RectDip bounds, GroupBoxOptions options = {});
    template <WindowLike Parent>
    bool Create(const Parent& parent, ControlId id, std::wstring_view text, RectDip bounds, GroupBoxOptions options = {}) {
        return Create(parent.GetHwnd(), id, text, bounds, options);
    }
};

struct ListBoxOptions {
    DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY;
    DWORD extended_style = WS_EX_CLIENTEDGE;
};

class ListBox final : public NativeControl {
public:
    bool Create(HWND parent, ControlId id, RectDip bounds, ListBoxOptions options = {});
    template <WindowLike Parent>
    bool Create(const Parent& parent, ControlId id, RectDip bounds, ListBoxOptions options = {}) {
        return Create(parent.GetHwnd(), id, bounds, options);
    }
    int AddItem(std::wstring_view text);
    int GetItemCount() const noexcept;
    std::optional<std::wstring> GetItemText(int index) const;
    bool RemoveItem(int index) noexcept;
    bool ClearItems() noexcept;
    int GetSelection() const noexcept;
    std::optional<int> GetSelectedIndex() const noexcept {
        const int index = GetSelection();
        return index == LB_ERR ? std::nullopt : std::optional<int>{index};
    }
    bool SetSelection(int index) noexcept;
};

struct ComboBoxOptions {
    DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL |
        CBS_DROPDOWNLIST;
    DWORD extended_style = 0;
};

class ComboBox final : public NativeControl {
public:
    bool Create(
        HWND parent,
        ControlId id,
        RectDip bounds,
        ComboBoxOptions options = {});
    template <WindowLike Parent>
    bool Create(
        const Parent& parent,
        ControlId id,
        RectDip bounds,
        ComboBoxOptions options = {}) {
        return Create(parent.GetHwnd(), id, bounds, options);
    }
    int AddItem(std::wstring_view text);
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

struct ProgressBarOptions {
    DWORD style = WS_CHILD | WS_VISIBLE | PBS_SMOOTH;
    DWORD extended_style = 0;
};

class ProgressBar final : public NativeControl {
public:
    bool Create(
        HWND parent,
        ControlId id,
        RectDip bounds,
        ProgressBarOptions options = {});
    template <WindowLike Parent>
    bool Create(
        const Parent& parent,
        ControlId id,
        RectDip bounds,
        ProgressBarOptions options = {}) {
        return Create(parent.GetHwnd(), id, bounds, options);
    }
    ProgressBar& SetRange(int minimum, int maximum) noexcept;
    ProgressBar& SetValue(int value) noexcept;
    int GetValue() const noexcept;
};

struct SliderOptions {
    DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | TBS_AUTOTICKS;
    DWORD extended_style = 0;
};

class Slider final : public NativeControl {
public:
    bool Create(HWND parent, ControlId id, RectDip bounds, SliderOptions options = {});
    template <WindowLike Parent>
    bool Create(const Parent& parent, ControlId id, RectDip bounds, SliderOptions options = {}) {
        return Create(parent.GetHwnd(), id, bounds, options);
    }
    Slider& SetRange(int minimum, int maximum) noexcept;
    Slider& SetValue(int value) noexcept;
    int GetValue() const noexcept;
};

struct ScrollBarOptions {
    DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | SBS_HORZ;
    DWORD extended_style = 0;
};

class ScrollBar final : public NativeControl {
public:
    bool Create(HWND parent, ControlId id, RectDip bounds, ScrollBarOptions options = {});
    template <WindowLike Parent>
    bool Create(const Parent& parent, ControlId id, RectDip bounds, ScrollBarOptions options = {}) { return Create(parent.GetHwnd(), id, bounds, options); }
    ScrollBar& SetRange(int minimum, int maximum) noexcept;
    ScrollBar& SetValue(int value) noexcept;
    int GetValue() const noexcept;
};

}  // namespace mwtl
