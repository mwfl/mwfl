#include <mwtl/appearance.h>
#include <mwtl/native_host.h>

#include <commctrl.h>

namespace mwtl {
namespace {

constexpr UINT_PTR kNativeHostSubclassId = 1;

DWORD ValidError(DWORD fallback) noexcept {
    const DWORD error = ::GetLastError();
    return error == ERROR_SUCCESS ? fallback : error;
}

}  // namespace

NativeHost::~NativeHost() noexcept {
    Destroy();
}

bool NativeHost::Create(HWND parent, ControlId id, RectDip bounds, NativeHostOptions options) {
    child_ = nullptr;
    model_ = {};
    if (!CreateNative(L"STATIC", parent, id, L"Native content host", bounds, options.style,
                      options.extended_style))
        return false;
    if (::SetWindowSubclass(GetHwnd(), SubclassProcedure, kNativeHostSubclassId,
                            reinterpret_cast<DWORD_PTR>(this)) == FALSE) {
        const DWORD error = ValidError(ERROR_FUNCTION_FAILED);
        Destroy();
        model_.MarkDestroyed();
        ::SetLastError(error);
        return false;
    }
    if (!model_.MarkCreated() ||
        !SetAccessibleName(GetHwnd(), L"Native content host")) {
        Destroy();
        model_.MarkDestroyed();
        ::SetLastError(ERROR_FUNCTION_FAILED);
        return false;
    }
    return true;
}

bool NativeHost::Attach(HWND child) noexcept {
    const HWND host = GetHwnd();
    if (host == nullptr || model_.GetState() != NativeHostState::ready) {
        ::SetLastError(ERROR_INVALID_STATE);
        return false;
    }
    if (child == nullptr || child == host || ::IsWindow(child) == FALSE ||
        ::GetParent(child) != host) {
        ::SetLastError(ERROR_INVALID_PARAMETER);
        return false;
    }
    DWORD host_process = 0;
    DWORD child_process = 0;
    const DWORD host_thread = ::GetWindowThreadProcessId(host, &host_process);
    const DWORD child_thread = ::GetWindowThreadProcessId(child, &child_process);
    if (host_thread == 0 || child_thread != host_thread || child_process != host_process) {
        ::SetLastError(ERROR_INVALID_THREAD_ID);
        return false;
    }
    if (!model_.Attach(reinterpret_cast<std::uintptr_t>(child))) {
        ::SetLastError(ERROR_INVALID_STATE);
        return false;
    }
    child_ = child;
    if (!Arrange()) {
        const DWORD error = ValidError(ERROR_FUNCTION_FAILED);
        child_ = nullptr;
        static_cast<void>(model_.Detach());
        ::SetLastError(error);
        return false;
    }
    return true;
}

HWND NativeHost::Detach() noexcept {
    if (model_.GetState() != NativeHostState::attached) {
        ::SetLastError(ERROR_INVALID_STATE);
        return nullptr;
    }
    const HWND child = child_;
    child_ = nullptr;
    static_cast<void>(model_.Detach());
    return child;
}

HWND NativeHost::GetChild() const noexcept {
    const HWND host = GetHwnd();
    return model_.IsAttached() && child_ != nullptr && ::IsWindow(child_) != FALSE &&
                   ::GetParent(child_) == host
               ? child_
               : nullptr;
}

bool NativeHost::SetBounds(RectDip bounds) noexcept {
    return NativeControl::SetBounds(bounds) && Arrange();
}

bool NativeHost::Arrange() noexcept {
    const HWND host = GetHwnd();
    if (host == nullptr) {
        ::SetLastError(ERROR_INVALID_WINDOW_HANDLE);
        return false;
    }
    if (!model_.IsAttached()) return true;
    const HWND child = GetChild();
    if (child == nullptr) {
        ObserveDetachedChild(child_);
        ::SetLastError(ERROR_INVALID_WINDOW_HANDLE);
        return false;
    }
    RECT client{};
    if (::GetClientRect(host, &client) == FALSE) return false;
    return ::SetWindowPos(child, nullptr, 0, 0, client.right - client.left,
                          client.bottom - client.top,
                          SWP_NOZORDER | SWP_NOACTIVATE) != FALSE;
}

bool NativeHost::IsAttachedSource(HWND source) const noexcept {
    const HWND child = GetChild();
    return source != nullptr && child != nullptr &&
           (source == child || ::IsChild(child, source) != FALSE);
}

void NativeHost::ObserveDetachedChild(HWND child) noexcept {
    if (child_ != child || !model_.IsAttached()) return;
    child_ = nullptr;
    static_cast<void>(model_.Detach());
}

LRESULT CALLBACK NativeHost::SubclassProcedure(HWND window, UINT message, WPARAM wparam,
                                               LPARAM lparam, UINT_PTR subclass_id,
                                               DWORD_PTR reference) noexcept {
    auto* self = reinterpret_cast<NativeHost*>(reference);
    if (self == nullptr) return ::DefSubclassProc(window, message, wparam, lparam);
    if (message == WM_NCDESTROY) {
        self->child_ = nullptr;
        self->model_.MarkDestroyed();
        ::RemoveWindowSubclass(window, SubclassProcedure, subclass_id);
        return ::DefSubclassProc(window, message, wparam, lparam);
    }
    return self->ProcessMessage(window, message, wparam, lparam);
}

LRESULT NativeHost::ProcessMessage(HWND window, UINT message, WPARAM wparam,
                                   LPARAM lparam) noexcept {
    const auto forward = [&]() noexcept {
        const HWND parent = ::GetParent(window);
        return parent == nullptr ? LRESULT{0}
                                 : ::SendMessageW(parent, message, wparam, lparam);
    };
    switch (message) {
        case WM_SIZE:
        case WM_DPICHANGED_AFTERPARENT:
            static_cast<void>(Arrange());
            return 0;
        case WM_SETFOCUS: {
            const HWND child = GetChild();
            if (child != nullptr) ::SetFocus(child);
            return 0;
        }
        case WM_NOTIFY: {
            const auto* header = reinterpret_cast<const NMHDR*>(lparam);
            if (header != nullptr && IsAttachedSource(header->hwndFrom)) return forward();
            break;
        }
        case WM_COMMAND:
            if (IsAttachedSource(reinterpret_cast<HWND>(lparam))) return forward();
            break;
        case WM_CONTEXTMENU:
            if (IsAttachedSource(reinterpret_cast<HWND>(wparam))) return forward();
            break;
        case WM_PARENTNOTIFY:
            if (LOWORD(wparam) == WM_DESTROY)
                ObserveDetachedChild(reinterpret_cast<HWND>(lparam));
            break;
        case WM_THEMECHANGED:
        case WM_SYSCOLORCHANGE:
        case WM_SETTINGCHANGE:
            ::RedrawWindow(window, nullptr, nullptr,
                           RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
            break;
        default:
            break;
    }
    return ::DefSubclassProc(window, message, wparam, lparam);
}

}  // namespace mwtl
