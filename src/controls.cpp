#include <mwfl/controls.h>
#include <wil/resource.h>

#include <utility>
#include <array>
#include <cassert>
#include <cwchar>

namespace mwfl {
namespace {

RECT ResolveControlBounds(HWND parent, RectDip bounds) noexcept {
    return DpiContext::FromWindow(parent).ToPixels(bounds);
}

void ApplyDefaultFont(HWND window, HWND parent) noexcept {
    HFONT font = parent != nullptr
        ? reinterpret_cast<HFONT>(::SendMessageW(parent, WM_GETFONT, 0, 0))
        : nullptr;
    if (font == nullptr)
        font = static_cast<HFONT>(::GetStockObject(DEFAULT_GUI_FONT));
    ::SendMessageW(window, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
}

}  // namespace

NativeControl::~NativeControl() noexcept {
    Destroy();
}

NativeControl::NativeControl(NativeControl&& other) noexcept
    : window_(std::exchange(other.window_, nullptr)),
      parent_(std::exchange(other.parent_, nullptr)),
      id_(std::exchange(other.id_, {})),
      owner_thread_id_(std::exchange(other.owner_thread_id_, 0)) {}

NativeControl& NativeControl::operator=(NativeControl&& other) noexcept {
    if (this != &other) {
        Destroy();
        window_ = std::exchange(other.window_, nullptr);
        parent_ = std::exchange(other.parent_, nullptr);
        id_ = std::exchange(other.id_, {});
        owner_thread_id_ = std::exchange(other.owner_thread_id_, 0);
    }
    return *this;
}

bool NativeControl::IsWindow() const noexcept {
    assert(IsOwnerThread());
    return window_ != nullptr && parent_ != nullptr && id_.value > 0 &&
        ::IsWindow(window_) != FALSE && ::IsWindow(parent_) != FALSE &&
        ::GetParent(window_) == parent_ && ::GetDlgCtrlID(window_) == id_.value;
}

bool NativeControl::IsOwnerThread() const noexcept {
    return owner_thread_id_ == 0 ||
        owner_thread_id_ == ::GetCurrentThreadId();
}

SizeDip MeasureNativeControl(HWND window, DpiContext dpi) {
    if (window == nullptr || ::IsWindow(window) == FALSE) return {};

    SIZE ideal{};
    wchar_t class_name[64]{};
    ::GetClassNameW(window, class_name, static_cast<int>(std::size(class_name)));
    if (_wcsicmp(class_name, L"Button") == 0 &&
        ::SendMessageW(window, BCM_GETIDEALSIZE, 0,
                       reinterpret_cast<LPARAM>(&ideal)) != FALSE &&
        ideal.cx > 0 && ideal.cy > 0) {
        return {dpi.FromPixels(ideal.cx), dpi.FromPixels(ideal.cy)};
    }
    if (_wcsicmp(class_name, TOOLBARCLASSNAMEW) == 0 &&
        ::SendMessageW(window, TB_GETMAXSIZE, 0,
                       reinterpret_cast<LPARAM>(&ideal)) != FALSE &&
        ideal.cx > 0 && ideal.cy > 0) {
        return {dpi.FromPixels(ideal.cx), dpi.FromPixels(ideal.cy)};
    }
    RECT ideal_rect{};
    if (_wcsicmp(class_name, MONTHCAL_CLASSW) == 0 &&
        ::SendMessageW(window, MCM_GETMINREQRECT, 0,
                       reinterpret_cast<LPARAM>(&ideal_rect)) != FALSE) {
        return {dpi.FromPixels(ideal_rect.right - ideal_rect.left),
                dpi.FromPixels(ideal_rect.bottom - ideal_rect.top)};
    }

    HDC dc = ::GetDC(window);
    if (dc == nullptr) return {80.0_dip, 24.0_dip};
    const auto release_dc = wil::scope_exit([&] { ::ReleaseDC(window, dc); });
    const HFONT font = reinterpret_cast<HFONT>(
        ::SendMessageW(window, WM_GETFONT, 0, 0));
    const HGDIOBJ old_font = font ? ::SelectObject(dc, font) : nullptr;
    const auto restore_font = wil::scope_exit([&] {
        if (old_font) ::SelectObject(dc, old_font);
    });
    const int text_length = ::GetWindowTextLengthW(window);
    std::wstring text(
        static_cast<std::size_t>((std::max)(text_length, 0)) + 1, L'\0');
    if (text_length > 0) {
        const int copied = ::GetWindowTextW(window, text.data(), text_length + 1);
        text.resize(static_cast<std::size_t>((std::max)(copied, 0)));
    } else {
        text.clear();
    }
    RECT text_bounds{0, 0, 0, 0};
    if (!text.empty()) {
        ::DrawTextW(dc, text.c_str(), static_cast<int>(text.size()),
                    &text_bounds, DT_CALCRECT | DT_SINGLELINE | DT_NOPREFIX);
    }
    TEXTMETRICW metrics{};
    ::GetTextMetricsW(dc, &metrics);
    int width = text_bounds.right - text_bounds.left;
    int height = (std::max)(static_cast<int>(text_bounds.bottom - text_bounds.top),
                            static_cast<int>(metrics.tmHeight));

    const DWORD style = static_cast<DWORD>(::GetWindowLongPtrW(window, GWL_STYLE));
    if (_wcsicmp(class_name, L"Static") == 0) {
        width += 2; height += 2;
    } else if (_wcsicmp(class_name, L"Edit") == 0) {
        width = (std::max)(width + 12, dpi.ToPixels(120.0_dip));
        height += 10;
    } else if (_wcsicmp(class_name, L"ComboBox") == 0 ||
               _wcsicmp(class_name, WC_COMBOBOXEXW) == 0) {
        width = (std::max)(width + 32, dpi.ToPixels(120.0_dip));
        height += 10;
    } else if (_wcsicmp(class_name, L"ListBox") == 0 ||
               _wcsicmp(class_name, WC_LISTVIEWW) == 0 ||
               _wcsicmp(class_name, WC_TREEVIEWW) == 0) {
        width = (std::max)(width + 24, dpi.ToPixels(160.0_dip));
        height = dpi.ToPixels(96.0_dip);
    } else if (_wcsicmp(class_name, PROGRESS_CLASSW) == 0 ||
               _wcsicmp(class_name, TRACKBAR_CLASSW) == 0 ||
               _wcsicmp(class_name, L"ScrollBar") == 0) {
        width = dpi.ToPixels(120.0_dip);
        height = dpi.ToPixels(24.0_dip);
    } else {
        width = (std::max)(width + 16, dpi.ToPixels(24.0_dip));
        height = (std::max)(height + 10, dpi.ToPixels(24.0_dip));
    }
    if ((style & WS_BORDER) != 0) { width += 2; height += 2; }
    return {dpi.FromPixels(width), dpi.FromPixels(height)};
}

SizeDip NativeControl::GetPreferredSize(DpiContext dpi) const {
    if (!IsWindow()) return {};
    return MeasureNativeControl(window_, dpi);
}

bool NativeControl::SetText(std::wstring_view text) {
    if (!IsWindow()) {
        ::SetLastError(ERROR_INVALID_WINDOW_HANDLE);
        return false;
    }
    const std::wstring terminated{text};
    return ::SetWindowTextW(window_, terminated.c_str()) != FALSE;
}

std::wstring NativeControl::GetText() const {
    if (!IsWindow()) {
        return {};
    }
    const int length = ::GetWindowTextLengthW(window_);
    if (length <= 0) {
        return {};
    }
    std::wstring text(static_cast<std::size_t>(length) + 1, L'\0');
    const int copied = ::GetWindowTextW(
        window_, text.data(), static_cast<int>(text.size()));
    text.resize(copied > 0 ? static_cast<std::size_t>(copied) : 0);
    return text;
}

bool NativeControl::SetBounds(RectDip bounds) noexcept {
    if (!IsWindow()) {
        ::SetLastError(ERROR_INVALID_WINDOW_HANDLE);
        return false;
    }
    const HWND parent = ::GetParent(window_);
    const RECT pixels = ResolveControlBounds(parent, bounds);
    return ::SetWindowPos(
               window_, nullptr, pixels.left, pixels.top,
               pixels.right - pixels.left, pixels.bottom - pixels.top,
               SWP_NOZORDER | SWP_NOACTIVATE) != FALSE;
}

NativeControl& NativeControl::SetEnabled(bool enabled) noexcept {
    if (IsWindow()) {
        ::EnableWindow(window_, enabled ? TRUE : FALSE);
    }
    return *this;
}

NativeControl& NativeControl::SetVisible(bool visible) noexcept {
    if (IsWindow()) {
        ::ShowWindow(window_, visible ? SW_SHOW : SW_HIDE);
    }
    return *this;
}

NativeControl& NativeControl::Focus() noexcept {
    if (IsWindow()) {
        ::SetFocus(window_);
    }
    return *this;
}

void NativeControl::Destroy() noexcept {
    if (IsWindow()) {
        ::DestroyWindow(window_);
    }
    window_ = nullptr;
    parent_ = nullptr;
    id_ = {};
    owner_thread_id_ = 0;
}

bool NativeControl::CreateNative(
    const wchar_t* class_name,
    HWND parent,
    ControlId id,
    std::wstring_view text,
    RectDip bounds,
    DWORD style,
    DWORD extended_style) {
    Destroy();
    if (parent == nullptr || id.value <= 0) {
        ::SetLastError(ERROR_INVALID_PARAMETER);
        return false;
    }

    const RECT pixels = ResolveControlBounds(parent, bounds);
    const std::wstring terminated{text};
    window_ = ::CreateWindowExW(
        extended_style,
        class_name,
        terminated.c_str(),
        style,
        pixels.left,
        pixels.top,
        pixels.right - pixels.left,
        pixels.bottom - pixels.top,
        parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id.value)),
        reinterpret_cast<HINSTANCE>(::GetWindowLongPtrW(parent, GWLP_HINSTANCE)),
        nullptr);
    if (window_ == nullptr) {
        return false;
    }
    parent_ = parent;
    id_ = id;
    owner_thread_id_ = ::GetCurrentThreadId();
    ApplyDefaultFont(window_, parent_);
    return true;
}

bool Label::Create(
    HWND parent,
    ControlId id,
    std::wstring_view text,
    RectDip bounds,
    LabelOptions options) {
    return CreateNative(
        L"STATIC", parent, id, text, bounds, options.style,
        options.extended_style);
}

bool Button::Create(
    HWND parent,
    ControlId id,
    std::wstring_view text,
    RectDip bounds,
    ButtonOptions options) {
    return CreateNative(
        L"BUTTON", parent, id, text, bounds, options.style,
        options.extended_style);
}

Button& Button::Click() noexcept {
    if (IsWindow()) {
        ::SendMessageW(GetHwnd(), BM_CLICK, 0, 0);
    }
    return *this;
}

bool TextBox::Create(
    HWND parent,
    ControlId id,
    std::wstring_view text,
    RectDip bounds,
    TextBoxOptions options) {
    return CreateNative(
        L"EDIT", parent, id, text, bounds, options.style,
        options.extended_style);
}

TextBox& TextBox::SelectAll() noexcept {
    if (IsWindow()) {
        ::SendMessageW(GetHwnd(), EM_SETSEL, 0, -1);
    }
    return *this;
}

TextBox& TextBox::SetReadOnly(bool read_only) noexcept {
    if (IsWindow()) {
        ::SendMessageW(GetHwnd(), EM_SETREADONLY, read_only ? TRUE : FALSE, 0);
    }
    return *this;
}

bool CheckBox::Create(
    HWND parent,
    ControlId id,
    std::wstring_view text,
    RectDip bounds,
    CheckBoxOptions options) {
    return CreateNative(
        L"BUTTON", parent, id, text, bounds, options.style,
        options.extended_style);
}

bool CheckBox::IsChecked() const noexcept {
    return IsWindow() &&
        ::SendMessageW(GetHwnd(), BM_GETCHECK, 0, 0) == BST_CHECKED;
}

CheckBox& CheckBox::SetChecked(bool checked) noexcept {
    if (IsWindow()) {
        ::SendMessageW(
            GetHwnd(), BM_SETCHECK, checked ? BST_CHECKED : BST_UNCHECKED, 0);
    }
    return *this;
}

bool RadioButton::Create(HWND parent, ControlId id, std::wstring_view text, RectDip bounds, RadioButtonOptions options) {
    return CreateNative(L"BUTTON", parent, id, text, bounds, options.style, options.extended_style);
}

bool RadioButton::IsChecked() const noexcept {
    return IsWindow() && ::SendMessageW(GetHwnd(), BM_GETCHECK, 0, 0) == BST_CHECKED;
}

RadioButton& RadioButton::SetChecked(bool checked) noexcept {
    if (IsWindow()) {
        ::SendMessageW(GetHwnd(), BM_SETCHECK, checked ? BST_CHECKED : BST_UNCHECKED, 0);
    }
    return *this;
}

bool GroupBox::Create(HWND parent, ControlId id, std::wstring_view text, RectDip bounds, GroupBoxOptions options) {
    return CreateNative(L"BUTTON", parent, id, text, bounds, options.style, options.extended_style);
}

bool ListBox::Create(HWND parent, ControlId id, RectDip bounds, ListBoxOptions options) {
    return CreateNative(L"LISTBOX", parent, id, L"", bounds, options.style, options.extended_style);
}

int ListBox::AddItem(std::wstring_view text) {
    if (!IsWindow()) {
        return LB_ERR;
    }
    const std::wstring terminated{text};
    return static_cast<int>(::SendMessageW(GetHwnd(), LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(terminated.c_str())));
}

int ListBox::GetItemCount() const noexcept {
    return IsWindow() ? static_cast<int>(::SendMessageW(GetHwnd(), LB_GETCOUNT, 0, 0)) : LB_ERR;
}

std::optional<std::wstring> ListBox::GetItemText(int index) const {
    if (!IsWindow()) {
        ::SetLastError(ERROR_INVALID_WINDOW_HANDLE);
        return std::nullopt;
    }
    const int count = GetItemCount();
    if (index < 0 || count == LB_ERR || index >= count) {
        ::SetLastError(ERROR_INVALID_INDEX);
        return std::nullopt;
    }
    const LRESULT length = ::SendMessageW(GetHwnd(), LB_GETTEXTLEN, index, 0);
    if (length == LB_ERR) return std::nullopt;
    std::wstring value(static_cast<std::size_t>(length) + 1, L'\0');
    const LRESULT copied = ::SendMessageW(GetHwnd(), LB_GETTEXT, index,
                                           reinterpret_cast<LPARAM>(value.data()));
    if (copied == LB_ERR) return std::nullopt;
    value.resize(static_cast<std::size_t>(copied));
    return value;
}

bool ListBox::RemoveItem(int index) noexcept {
    if (!IsWindow()) {
        ::SetLastError(ERROR_INVALID_WINDOW_HANDLE);
        return false;
    }
    const int count = GetItemCount();
    if (index < 0 || count == LB_ERR || index >= count) {
        ::SetLastError(ERROR_INVALID_INDEX);
        return false;
    }
    return ::SendMessageW(GetHwnd(), LB_DELETESTRING, index, 0) != LB_ERR;
}

bool ListBox::ClearItems() noexcept {
    if (!IsWindow()) {
        ::SetLastError(ERROR_INVALID_WINDOW_HANDLE);
        return false;
    }
    ::SendMessageW(GetHwnd(), LB_RESETCONTENT, 0, 0);
    return true;
}

int ListBox::GetSelection() const noexcept {
    return IsWindow() ? static_cast<int>(::SendMessageW(GetHwnd(), LB_GETCURSEL, 0, 0)) : LB_ERR;
}

bool ListBox::SetSelection(int index) noexcept {
    return IsWindow() && ::SendMessageW(GetHwnd(), LB_SETCURSEL, index, 0) != LB_ERR;
}

bool ComboBox::Create(
    HWND parent,
    ControlId id,
    RectDip bounds,
    ComboBoxOptions options) {
    return CreateNative(
        L"COMBOBOX", parent, id, L"", bounds, options.style,
        options.extended_style);
}

int ComboBox::AddItem(std::wstring_view text) {
    if (!IsWindow()) {
        return CB_ERR;
    }
    const std::wstring terminated{text};
    return static_cast<int>(::SendMessageW(
        GetHwnd(), CB_ADDSTRING, 0,
        reinterpret_cast<LPARAM>(terminated.c_str())));
}

int ComboBox::GetItemCount() const noexcept {
    return IsWindow() ? static_cast<int>(::SendMessageW(GetHwnd(), CB_GETCOUNT, 0, 0)) : CB_ERR;
}

std::optional<std::wstring> ComboBox::GetItemText(int index) const {
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

bool ComboBox::RemoveItem(int index) noexcept {
    if (!IsWindow()) {
        ::SetLastError(ERROR_INVALID_WINDOW_HANDLE);
        return false;
    }
    const int count = GetItemCount();
    if (index < 0 || count == CB_ERR || index >= count) {
        ::SetLastError(ERROR_INVALID_INDEX);
        return false;
    }
    return ::SendMessageW(GetHwnd(), CB_DELETESTRING, index, 0) != CB_ERR;
}

bool ComboBox::ClearItems() noexcept {
    if (!IsWindow()) {
        ::SetLastError(ERROR_INVALID_WINDOW_HANDLE);
        return false;
    }
    ::SendMessageW(GetHwnd(), CB_RESETCONTENT, 0, 0);
    return true;
}

int ComboBox::GetSelection() const noexcept {
    return IsWindow()
        ? static_cast<int>(::SendMessageW(GetHwnd(), CB_GETCURSEL, 0, 0))
        : CB_ERR;
}

bool ComboBox::SetSelection(int index) noexcept {
    return IsWindow() &&
        ::SendMessageW(GetHwnd(), CB_SETCURSEL, index, 0) != CB_ERR;
}

bool ProgressBar::Create(
    HWND parent,
    ControlId id,
    RectDip bounds,
    ProgressBarOptions options) {
    INITCOMMONCONTROLSEX controls{
        sizeof(controls), ICC_PROGRESS_CLASS};
    if (::InitCommonControlsEx(&controls) == FALSE) {
        return false;
    }
    return CreateNative(
        PROGRESS_CLASSW, parent, id, L"", bounds, options.style,
        options.extended_style);
}

ProgressBar& ProgressBar::SetRange(int minimum, int maximum) noexcept {
    if (IsWindow()) {
        ::SendMessageW(GetHwnd(), PBM_SETRANGE32, minimum, maximum);
    }
    return *this;
}

ProgressBar& ProgressBar::SetValue(int value) noexcept {
    if (IsWindow()) {
        ::SendMessageW(GetHwnd(), PBM_SETPOS, value, 0);
    }
    return *this;
}

int ProgressBar::GetValue() const noexcept {
    return IsWindow()
        ? static_cast<int>(::SendMessageW(GetHwnd(), PBM_GETPOS, 0, 0))
        : 0;
}

bool Slider::Create(HWND parent, ControlId id, RectDip bounds, SliderOptions options) {
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_BAR_CLASSES};
    if (::InitCommonControlsEx(&controls) == FALSE) {
        return false;
    }
    return CreateNative(TRACKBAR_CLASSW, parent, id, L"", bounds, options.style, options.extended_style);
}

Slider& Slider::SetRange(int minimum, int maximum) noexcept {
    if (IsWindow()) {
        ::SendMessageW(GetHwnd(), TBM_SETRANGEMIN, FALSE, minimum);
        ::SendMessageW(GetHwnd(), TBM_SETRANGEMAX, TRUE, maximum);
    }
    return *this;
}

Slider& Slider::SetValue(int value) noexcept {
    if (IsWindow()) {
        ::SendMessageW(GetHwnd(), TBM_SETPOS, TRUE, value);
    }
    return *this;
}

int Slider::GetValue() const noexcept {
    return IsWindow() ? static_cast<int>(::SendMessageW(GetHwnd(), TBM_GETPOS, 0, 0)) : 0;
}

bool ScrollBar::Create(HWND parent, ControlId id, RectDip bounds, ScrollBarOptions options) {
    return CreateNative(L"SCROLLBAR", parent, id, L"", bounds, options.style, options.extended_style);
}
ScrollBar& ScrollBar::SetRange(int minimum, int maximum) noexcept { if (IsWindow()) ::SetScrollRange(GetHwnd(), SB_CTL, minimum, maximum, TRUE); return *this; }
ScrollBar& ScrollBar::SetValue(int value) noexcept { if (IsWindow()) ::SetScrollPos(GetHwnd(), SB_CTL, value, TRUE); return *this; }
int ScrollBar::GetValue() const noexcept { return IsWindow() ? ::GetScrollPos(GetHwnd(), SB_CTL) : 0; }

}  // namespace mwfl
