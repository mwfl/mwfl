#include <mwtl/dialog.h>

#include <windows.h>

// ATL requires this include order.
// clang-format off
#include <atlbase.h>
#include <atlapp.h>
// clang-format on

#include <algorithm>
#include <stdexcept>
#include <utility>

extern WTL::CAppModule _Module;

namespace mwtl {
namespace {

constexpr UINT kAcceptDialog = WM_APP + 0x51B;
constexpr UINT kCancelDialog = WM_APP + 0x51C;

struct BlankDialogTemplate {
    DLGTEMPLATE dialog{};
    WORD menu = 0;
    WORD window_class = 0;
    WORD title = 0;
    WORD point_size = 9;
    wchar_t typeface[9] = L"Segoe UI";

    BlankDialogTemplate(bool resizable) noexcept {
        dialog.style = WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME | DS_SETFONT |
                       WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
        if (resizable) dialog.style |= WS_THICKFRAME | WS_MAXIMIZEBOX;
        dialog.dwExtendedStyle = WS_EX_CONTROLPARENT;
        dialog.cdit = 0;
        dialog.x = 0;
        dialog.y = 0;
        dialog.cx = 200;
        dialog.cy = 120;
    }
};

}  // namespace

struct Dialog::State {
    class Filter final : public WTL::CMessageFilter {
       public:
        explicit Filter(State& input) noexcept : state(input) {}
        BOOL PreTranslateMessage(MSG* message) override {
            return message != nullptr && state.window != nullptr &&
                           ::IsWindow(state.window) != FALSE
                       ? ::IsDialogMessageW(state.window, message)
                       : FALSE;
        }

       private:
        State& state;
    };

    explicit State(DialogOptions input)
        : options(std::move(input)), dialog_template(options.resizable), filter(*this) {}

    void RecordException() noexcept {
        if (!result.callback_exception) result.callback_exception = std::current_exception();
        result.status = DialogStatus::failed;
        result.error = ERROR_UNHANDLED_EXCEPTION;
    }

    void RegisterFilter() noexcept {
        if (filter_registered || _Module.m_pMsgLoopMap == nullptr) return;
        WTL::CMessageLoop* loop = _Module.GetMessageLoop();
        if (loop != nullptr) filter_registered = loop->AddMessageFilter(&filter) != FALSE;
    }

    void UnregisterFilter() noexcept {
        if (!filter_registered || _Module.m_pMsgLoopMap == nullptr) return;
        WTL::CMessageLoop* loop = _Module.GetMessageLoop();
        if (loop != nullptr) loop->RemoveMessageFilter(&filter);
        filter_registered = false;
    }

    void RestoreOwner() noexcept {
        if (!owner_disabled) return;
        owner_disabled = false;
        if (options.owner != nullptr && ::IsWindow(options.owner) != FALSE) {
            ::EnableWindow(options.owner, TRUE);
            ::SetActiveWindow(options.owner);
        }
    }

    bool Arrange() noexcept {
        if (layout && window != nullptr && !layout->Arrange(window) && result.error == 0) {
            result.status = DialogStatus::failed;
            result.error = ::GetLastError();
            if (result.error == ERROR_SUCCESS) result.error = ERROR_FUNCTION_FAILED;
            return false;
        }
        return result.status != DialogStatus::failed;
    }

    bool Finish(DialogStatus status, INT_PTR command_id) noexcept {
        if (window == nullptr || ::IsWindow(window) == FALSE) return false;
        result.status = status;
        result.command_id = command_id;
        if (modeless) return ::DestroyWindow(window) != FALSE;
        return ::EndDialog(window, command_id) != FALSE;
    }

    DialogOptions options;
    BlankDialogTemplate dialog_template;
    std::unique_ptr<LayoutHost> layout;
    DialogResult result{};
    Filter filter;
    std::shared_ptr<State> keep_alive;
    HWND window = nullptr;  // Owned while modeless; native modal call owns otherwise.
    DWORD owner_thread = 0;
    int minimum_width = 0;
    int minimum_height = 0;
    bool modeless = false;
    bool filter_registered = false;
    bool owner_disabled = false;
};

Dialog::Dialog(DialogOptions options) : state_(std::make_shared<State>(std::move(options))) {
    if (state_->options.initial_client_size.width.value <= 0.0f ||
        state_->options.initial_client_size.height.value <= 0.0f) {
        throw std::invalid_argument("dialog initial size must be positive");
    }
}

Dialog::~Dialog() noexcept {
    Close();
}

Dialog::Dialog(Dialog&& other) noexcept : state_(std::move(other.state_)) {}

Dialog& Dialog::operator=(Dialog&& other) noexcept {
    if (this == &other) return *this;
    Close();
    state_ = std::move(other.state_);
    return *this;
}

bool Dialog::SetLayout(LayoutNode root) {
    if (!state_) return false;
    state_->layout = std::make_unique<LayoutHost>(std::move(root));
    if (state_->window == nullptr) return true;
    return state_->layout->Arrange(state_->window);
}

void Dialog::ClearLayout() noexcept {
    if (state_) state_->layout.reset();
}

DialogResult Dialog::ShowModal() noexcept {
    if (!state_ || state_->window != nullptr) {
        DialogResult failed{};
        failed.status = DialogStatus::failed;
        failed.error = ERROR_INVALID_STATE;
        return failed;
    }
    state_->result = {};
    state_->result.status = DialogStatus::cancelled;
    state_->modeless = false;
    state_->owner_thread = ::GetCurrentThreadId();
    const INT_PTR native_result = ::DialogBoxIndirectParamW(
        ::GetModuleHandleW(nullptr), &state_->dialog_template.dialog, state_->options.owner,
        Procedure, reinterpret_cast<LPARAM>(state_.get()));
    if (native_result == -1) {
        state_->result.status = DialogStatus::failed;
        state_->result.error = ::GetLastError();
        if (state_->result.error == ERROR_SUCCESS) state_->result.error = ERROR_FUNCTION_FAILED;
    } else {
        state_->result.command_id = native_result;
    }
    state_->window = nullptr;
    state_->UnregisterFilter();
    state_->RestoreOwner();
    return state_->result;
}

bool Dialog::CreateModeless() noexcept {
    if (!state_ || state_->window != nullptr) {
        ::SetLastError(ERROR_INVALID_STATE);
        return false;
    }
    if (state_->options.disable_owner_for_modeless &&
        (state_->options.owner == nullptr || ::IsWindow(state_->options.owner) == FALSE)) {
        ::SetLastError(ERROR_INVALID_WINDOW_HANDLE);
        return false;
    }
    state_->result = {};
    state_->result.status = DialogStatus::cancelled;
    state_->modeless = true;
    state_->owner_thread = ::GetCurrentThreadId();
    state_->keep_alive = state_;
    const HWND window = ::CreateDialogIndirectParamW(
        ::GetModuleHandleW(nullptr), &state_->dialog_template.dialog, state_->options.owner,
        Procedure, reinterpret_cast<LPARAM>(state_.get()));
    if (window == nullptr || ::IsWindow(window) == FALSE) {
        state_->keep_alive.reset();
        state_->window = nullptr;
        if (state_->result.status != DialogStatus::failed) {
            state_->result.status = DialogStatus::failed;
            state_->result.error = ::GetLastError();
        }
        return false;
    }
    if (state_->options.disable_owner_for_modeless) {
        ::EnableWindow(state_->options.owner, FALSE);
        state_->owner_disabled = true;
    }
    state_->RegisterFilter();
    ::ShowWindow(window, SW_SHOWNORMAL);
    return true;
}

DialogResult Dialog::GetResult() const noexcept {
    return state_ ? state_->result : DialogResult{};
}

HWND Dialog::GetHwnd() const noexcept {
    return state_ && state_->window != nullptr && ::IsWindow(state_->window) != FALSE
               ? state_->window
               : nullptr;
}

bool Dialog::IsWindow() const noexcept {
    return GetHwnd() != nullptr;
}

bool Dialog::IsOwnerThread() const noexcept {
    return state_ && state_->owner_thread != 0 && state_->owner_thread == ::GetCurrentThreadId();
}

bool Dialog::IsModeless() const noexcept {
    return state_ && state_->modeless && IsWindow();
}

bool Dialog::Accept(INT_PTR command_id) noexcept {
    if (!state_ || !IsWindow()) return false;
    if (!IsOwnerThread()) {
        return ::PostMessageW(state_->window, kAcceptDialog, static_cast<WPARAM>(command_id), 0) !=
               FALSE;
    }
    return state_->Finish(DialogStatus::accepted, command_id);
}

bool Dialog::Cancel(INT_PTR command_id) noexcept {
    if (!state_ || !IsWindow()) return false;
    if (!IsOwnerThread()) {
        return ::PostMessageW(state_->window, kCancelDialog, static_cast<WPARAM>(command_id), 0) !=
               FALSE;
    }
    return state_->Finish(DialogStatus::cancelled, command_id);
}

void Dialog::Close() noexcept {
    if (!state_) return;
    if (IsWindow()) static_cast<void>(Cancel());
    if (!IsWindow()) {
        state_->UnregisterFilter();
        state_->RestoreOwner();
    }
    state_.reset();
}

INT_PTR CALLBACK Dialog::Procedure(HWND window, UINT message, WPARAM wparam,
                                   LPARAM lparam) noexcept {
    State* state = reinterpret_cast<State*>(::GetWindowLongPtrW(window, DWLP_USER));
    if (message == WM_INITDIALOG) {
        state = reinterpret_cast<State*>(lparam);
        if (state == nullptr) return FALSE;
        ::SetWindowLongPtrW(window, DWLP_USER, reinterpret_cast<LONG_PTR>(state));
        state->window = window;
        state->owner_thread = ::GetCurrentThreadId();
        try {
            if (!state->options.title.empty())
                ::SetWindowTextW(window, state->options.title.c_str());
            const DpiContext dpi = DpiContext::FromWindow(window);
            RECT bounds{0, 0, dpi.ToPixels(state->options.initial_client_size.width),
                        dpi.ToPixels(state->options.initial_client_size.height)};
            ::AdjustWindowRectExForDpi(
                &bounds, static_cast<DWORD>(::GetWindowLongPtrW(window, GWL_STYLE)), FALSE,
                static_cast<DWORD>(::GetWindowLongPtrW(window, GWL_EXSTYLE)), dpi.GetDpi());
            ::SetWindowPos(window, nullptr, 0, 0, bounds.right - bounds.left,
                           bounds.bottom - bounds.top, SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOZORDER);
            RECT actual{};
            ::GetWindowRect(window, &actual);
            state->minimum_width = actual.right - actual.left;
            state->minimum_height = actual.bottom - actual.top;
            if (state->options.callbacks.initialize &&
                !state->options.callbacks.initialize(window)) {
                state->result.status = DialogStatus::failed;
                state->result.error = ERROR_FUNCTION_FAILED;
                state->Finish(DialogStatus::failed, 0);
                return FALSE;
            }
            if (!state->Arrange()) {
                state->Finish(DialogStatus::failed, 0);
                return FALSE;
            }
            return TRUE;
        } catch (...) {
            state->RecordException();
            state->Finish(DialogStatus::failed, 0);
            return FALSE;
        }
    }
    if (state == nullptr) return FALSE;
    if (message == WM_NCDESTROY) {
        const std::shared_ptr<State> keep_alive = state->keep_alive;
        state->UnregisterFilter();
        state->RestoreOwner();
        state->window = nullptr;
        try {
            if (state->options.callbacks.destroyed) state->options.callbacks.destroyed();
        } catch (...) {
            state->RecordException();
        }
        state->keep_alive.reset();
        ::SetWindowLongPtrW(window, DWLP_USER, 0);
        return FALSE;
    }
    try {
        if (message == kAcceptDialog) {
            state->Finish(DialogStatus::accepted, static_cast<INT_PTR>(wparam));
            return TRUE;
        }
        if (message == kCancelDialog) {
            state->Finish(DialogStatus::cancelled, static_cast<INT_PTR>(wparam));
            return TRUE;
        }
        if (message == WM_GETMINMAXINFO && state->options.resizable) {
            auto* limits = reinterpret_cast<MINMAXINFO*>(lparam);
            if (limits != nullptr) {
                limits->ptMinTrackSize.x = state->minimum_width;
                limits->ptMinTrackSize.y = state->minimum_height;
            }
            return TRUE;
        }
        if (message == WM_SIZE || message == WM_DPICHANGED_AFTERPARENT) {
            if (!state->Arrange()) state->Finish(DialogStatus::failed, 0);
            return TRUE;
        }
        if (message == WM_COMMAND) {
            const WORD id = LOWORD(wparam);
            const WORD notification = HIWORD(wparam);
            if (state->options.callbacks.command &&
                state->options.callbacks.command(window, id, notification)) {
                return TRUE;
            }
            if (id == IDOK) {
                state->Finish(DialogStatus::accepted, IDOK);
                return TRUE;
            }
            if (id == IDCANCEL) {
                state->Finish(DialogStatus::cancelled, IDCANCEL);
                return TRUE;
            }
        }
        if (message == WM_CLOSE) {
            if (state->options.callbacks.close && !state->options.callbacks.close(window)) {
                return TRUE;
            }
            state->Finish(DialogStatus::cancelled, IDCANCEL);
            return TRUE;
        }
    } catch (...) {
        state->RecordException();
        state->Finish(DialogStatus::failed, 0);
        return TRUE;
    }
    return FALSE;
}

}  // namespace mwtl
