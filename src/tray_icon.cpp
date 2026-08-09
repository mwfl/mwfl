#include <mwtl/tray_icon.h>

#include <windowsx.h>

#include <cwchar>
#include <iterator>
#include <limits>
#include <utility>

namespace mwtl {
namespace {

bool IsNullGuid(const GUID& value) noexcept {
    return ::IsEqualGUID(value, GUID_NULL) != FALSE;
}

template <std::size_t Size>
bool CopyText(wchar_t (&destination)[Size], std::wstring_view source) noexcept {
    if (source.size() >= Size) {
        ::SetLastError(ERROR_BUFFER_OVERFLOW);
        return false;
    }
    if (!source.empty()) std::wmemcpy(destination, source.data(), source.size());
    destination[source.size()] = L'\0';
    return true;
}

bool ShellNotify(DWORD operation, NOTIFYICONDATAW& data) noexcept {
    ::SetLastError(ERROR_SUCCESS);
    if (::Shell_NotifyIconW(operation, &data) != FALSE) return true;
    if (::GetLastError() == ERROR_SUCCESS) ::SetLastError(ERROR_FUNCTION_FAILED);
    return false;
}

TrayIconEventKind DecodeKind(UINT code) noexcept {
    switch (code) {
        case NIN_SELECT:
        case NIN_KEYSELECT:
        case WM_LBUTTONUP:
        case WM_LBUTTONDBLCLK:
            return TrayIconEventKind::primary_activate;
        case WM_CONTEXTMENU:
        case WM_RBUTTONUP:
            return TrayIconEventKind::context_menu;
        case NIN_BALLOONSHOW:
            return TrayIconEventKind::balloon_shown;
        case NIN_BALLOONHIDE:
            return TrayIconEventKind::balloon_hidden;
        case NIN_BALLOONTIMEOUT:
            return TrayIconEventKind::balloon_timed_out;
        case NIN_BALLOONUSERCLICK:
            return TrayIconEventKind::balloon_clicked;
        case NIN_POPUPOPEN:
            return TrayIconEventKind::popup_opened;
        case NIN_POPUPCLOSE:
            return TrayIconEventKind::popup_closed;
        default:
            return TrayIconEventKind::unknown;
    }
}

}  // namespace

TrayIcon::~TrayIcon() noexcept {
    Remove();
}

TrayIcon::TrayIcon(TrayIcon&& other) noexcept
    : options_(std::move(other.options_)),
      state_(std::exchange(other.state_, TrayIconState::detached)),
      owner_thread_(std::exchange(other.owner_thread_, 0)) {}

TrayIcon& TrayIcon::operator=(TrayIcon&& other) noexcept {
    if (this == &other) return *this;
    Remove();
    options_ = std::move(other.options_);
    state_ = std::exchange(other.state_, TrayIconState::detached);
    owner_thread_ = std::exchange(other.owner_thread_, 0);
    return *this;
}

bool TrayIcon::ValidateThread() const noexcept {
    if (owner_thread_ == 0 || owner_thread_ == ::GetCurrentThreadId()) return true;
    ::SetLastError(ERROR_INVALID_THREAD_ID);
    return false;
}

bool TrayIcon::IsOwnerThread() const noexcept {
    return owner_thread_ == 0 || owner_thread_ == ::GetCurrentThreadId();
}

NOTIFYICONDATAW TrayIcon::BaseData() const noexcept {
    NOTIFYICONDATAW data{};
    data.cbSize = sizeof(data);
    data.hWnd = options_.owner;
    data.uID = options_.id;
    data.guidItem = options_.identity;
    data.uFlags = NIF_GUID;
    return data;
}

bool TrayIcon::Add(TrayIconOptions options) noexcept {
    if (state_ != TrayIconState::detached) {
        ::SetLastError(ERROR_ALREADY_EXISTS);
        return false;
    }
    if (options.owner == nullptr || ::IsWindow(options.owner) == FALSE || options.id == 0 ||
        options.id > static_cast<UINT>((std::numeric_limits<WORD>::max)()) ||
        options.callback_message < WM_APP || options.callback_message > 0xBFFF ||
        IsNullGuid(options.identity) || options.icon == nullptr ||
        options.tooltip.size() >= std::size(NOTIFYICONDATAW{}.szTip)) {
        ::SetLastError(ERROR_INVALID_PARAMETER);
        return false;
    }
    options_ = std::move(options);
    owner_thread_ = ::GetCurrentThreadId();
    state_ = TrayIconState::recovery_pending;
    if (AddNative()) return true;
    options_ = {};
    state_ = TrayIconState::detached;
    owner_thread_ = 0;
    return false;
}

bool TrayIcon::AddNative() noexcept {
    if (!ValidateThread()) return false;
    NOTIFYICONDATAW data = BaseData();
    data.uFlags |= NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_STATE | NIF_SHOWTIP;
    data.uCallbackMessage = options_.callback_message;
    data.hIcon = options_.icon;
    data.dwStateMask = NIS_HIDDEN;
    data.dwState = options_.hidden ? NIS_HIDDEN : 0;
    if (!CopyText(data.szTip, options_.tooltip) || !ShellNotify(NIM_ADD, data)) {
        state_ = TrayIconState::recovery_pending;
        return false;
    }
    data.uVersion = NOTIFYICON_VERSION_4;
    if (!ShellNotify(NIM_SETVERSION, data)) {
        static_cast<void>(ShellNotify(NIM_DELETE, data));
        state_ = TrayIconState::recovery_pending;
        return false;
    }
    state_ = TrayIconState::added;
    return true;
}

bool TrayIcon::Modify(NOTIFYICONDATAW& data) noexcept {
    if (!ValidateThread() || state_ != TrayIconState::added) {
        if (state_ != TrayIconState::added) ::SetLastError(ERROR_INVALID_STATE);
        return false;
    }
    return ShellNotify(NIM_MODIFY, data);
}

bool TrayIcon::UpdateTooltip(std::wstring_view tooltip) noexcept {
    if (tooltip.size() >= std::size(NOTIFYICONDATAW{}.szTip)) {
        ::SetLastError(ERROR_BUFFER_OVERFLOW);
        return false;
    }
    NOTIFYICONDATAW data = BaseData();
    data.uFlags |= NIF_TIP | NIF_SHOWTIP;
    if (!CopyText(data.szTip, tooltip) || !Modify(data)) return false;
    options_.tooltip.assign(tooltip);
    return true;
}

bool TrayIcon::UpdateIcon(HICON icon) noexcept {
    if (icon == nullptr) {
        ::SetLastError(ERROR_INVALID_PARAMETER);
        return false;
    }
    NOTIFYICONDATAW data = BaseData();
    data.uFlags |= NIF_ICON;
    data.hIcon = icon;
    if (!Modify(data)) return false;
    options_.icon = icon;
    return true;
}

bool TrayIcon::SetVisible(bool visible) noexcept {
    NOTIFYICONDATAW data = BaseData();
    data.uFlags |= NIF_STATE;
    data.dwStateMask = NIS_HIDDEN;
    data.dwState = visible ? 0 : NIS_HIDDEN;
    if (!Modify(data)) return false;
    options_.hidden = !visible;
    return true;
}

bool TrayIcon::ShowNotification(const TrayNotification& notification) noexcept {
    if (notification.title.size() >= std::size(NOTIFYICONDATAW{}.szInfoTitle) ||
        notification.text.size() >= std::size(NOTIFYICONDATAW{}.szInfo)) {
        ::SetLastError(ERROR_BUFFER_OVERFLOW);
        return false;
    }
    NOTIFYICONDATAW data = BaseData();
    data.uFlags |= NIF_INFO;
    data.dwInfoFlags = notification.flags;
    if (notification.respect_quiet_time) data.dwInfoFlags |= NIIF_RESPECT_QUIET_TIME;
    if (!CopyText(data.szInfoTitle, notification.title) ||
        !CopyText(data.szInfo, notification.text))
        return false;
    return Modify(data);
}

bool TrayIcon::Recreate() noexcept {
    if (!ValidateThread() || state_ == TrayIconState::detached) {
        if (state_ == TrayIconState::detached) ::SetLastError(ERROR_INVALID_STATE);
        return false;
    }
    NOTIFYICONDATAW stale = BaseData();
    static_cast<void>(ShellNotify(NIM_DELETE, stale));
    state_ = TrayIconState::recovery_pending;
    return AddNative();
}

bool TrayIcon::IsTaskbarCreated(const WindowMessage& message) const noexcept {
    const UINT registered = GetTaskbarCreatedMessage();
    return registered != 0 && message.id == registered;
}

std::optional<TrayIconEvent> TrayIcon::Decode(const WindowMessage& message) const noexcept {
    if (state_ != TrayIconState::added || message.id != options_.callback_message) {
        return std::nullopt;
    }
    const UINT code = LOWORD(message.lparam);
    const UINT id = HIWORD(message.lparam);
    if (id != LOWORD(options_.id)) return std::nullopt;
    return TrayIconEvent{DecodeKind(code),
                         code,
                         {GET_X_LPARAM(message.wparam), GET_Y_LPARAM(message.wparam)},
                         code == NIN_KEYSELECT};
}

void TrayIcon::Remove() noexcept {
    if (state_ == TrayIconState::detached) return;
    NOTIFYICONDATAW data = BaseData();
    static_cast<void>(ShellNotify(NIM_DELETE, data));
    options_ = {};
    state_ = TrayIconState::detached;
    owner_thread_ = 0;
}

UINT TrayIcon::GetTaskbarCreatedMessage() noexcept {
    static const UINT message = ::RegisterWindowMessageW(L"TaskbarCreated");
    return message;
}

}  // namespace mwtl
