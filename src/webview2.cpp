#include <mwtl/webview2.h>

#include <mwtl/appearance.h>

#include <WebView2.h>
#include <commctrl.h>
#include <wrl.h>

#include <atomic>
#include <utility>

namespace mwtl {
namespace {
using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

constexpr UINT_PTR kWebView2SubclassId = 1;

WebView2ProcessFailureKind TranslateFailure(COREWEBVIEW2_PROCESS_FAILED_KIND kind) noexcept {
    switch (kind) {
        case COREWEBVIEW2_PROCESS_FAILED_KIND_BROWSER_PROCESS_EXITED:
            return WebView2ProcessFailureKind::browser_process_exited;
        case COREWEBVIEW2_PROCESS_FAILED_KIND_RENDER_PROCESS_EXITED:
            return WebView2ProcessFailureKind::render_process_exited;
        case COREWEBVIEW2_PROCESS_FAILED_KIND_RENDER_PROCESS_UNRESPONSIVE:
            return WebView2ProcessFailureKind::render_process_unresponsive;
        case COREWEBVIEW2_PROCESS_FAILED_KIND_FRAME_RENDER_PROCESS_EXITED:
            return WebView2ProcessFailureKind::frame_render_process_exited;
        case COREWEBVIEW2_PROCESS_FAILED_KIND_UTILITY_PROCESS_EXITED:
            return WebView2ProcessFailureKind::utility_process_exited;
        case COREWEBVIEW2_PROCESS_FAILED_KIND_SANDBOX_HELPER_PROCESS_EXITED:
            return WebView2ProcessFailureKind::sandbox_helper_process_exited;
        default:
            return WebView2ProcessFailureKind::unknown;
    }
}

UINT TranslateKeyMessage(COREWEBVIEW2_KEY_EVENT_KIND kind) noexcept {
    switch (kind) {
        case COREWEBVIEW2_KEY_EVENT_KIND_KEY_DOWN: return WM_KEYDOWN;
        case COREWEBVIEW2_KEY_EVENT_KIND_KEY_UP: return WM_KEYUP;
        case COREWEBVIEW2_KEY_EVENT_KIND_SYSTEM_KEY_DOWN: return WM_SYSKEYDOWN;
        case COREWEBVIEW2_KEY_EVENT_KIND_SYSTEM_KEY_UP: return WM_SYSKEYUP;
        default: return 0;
    }
}

}  // namespace

WebView2RuntimeInfo ClassifyWebView2RuntimeResult(HRESULT result,
                                                  std::wstring version) noexcept {
    if (FAILED(result)) {
        const bool missing = result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) ||
                             result == HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND);
        return {missing ? WebView2RuntimeStatus::missing : WebView2RuntimeStatus::failed,
                result, {}};
    }
    return {WebView2RuntimeStatus::available, S_OK, std::move(version)};
}

WebView2RuntimeInfo QueryWebView2Runtime() noexcept {
    LPWSTR version = nullptr;
    const HRESULT result = ::GetAvailableCoreWebView2BrowserVersionString(nullptr, &version);
    std::wstring value = version == nullptr ? std::wstring{} : std::wstring{version};
    ::CoTaskMemFree(version);
    return ClassifyWebView2RuntimeResult(result, std::move(value));
}

struct WebView2Host::Implementation final {
    struct CallbackState final {
        std::atomic<Implementation*> owner = nullptr;
    };

    explicit Implementation(WebView2Host& host)
        : host(host), owner_thread(::GetCurrentThreadId()), callback_state(std::make_shared<CallbackState>()) {
        callback_state->owner.store(this);
    }

    ~Implementation() { callback_state->owner.store(nullptr); }

    bool IsOwnerThread() const noexcept { return ::GetCurrentThreadId() == owner_thread; }

    void Fail(const AsyncInitializationTicket& ticket, WebView2RuntimeStatus runtime,
              HRESULT error) noexcept {
        if (!IsOwnerThread() || state != WebView2HostState::initializing) return;
        static_cast<void>(initialization.Fail(ticket, error));
        state = WebView2HostState::failed;
        result = {state, runtime, FAILED(error) ? error : E_FAIL};
        try {
            if (callbacks.initialized) callbacks.initialized(result);
        } catch (...) {
            result.error = E_UNEXPECTED;
        }
    }

    void RegisterEvents() {
        const std::weak_ptr<CallbackState> weak = callback_state;
        auto navigation = Callback<ICoreWebView2NavigationCompletedEventHandler>(
            [weak](ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs* args) noexcept {
                const auto state = weak.lock();
                auto* self = state ? state->owner.load() : nullptr;
                if (self == nullptr || !self->callbacks.navigation_completed) return S_OK;
                BOOL succeeded = FALSE;
                const HRESULT read = args == nullptr ? E_POINTER : args->get_IsSuccess(&succeeded);
                try { self->callbacks.navigation_completed(SUCCEEDED(read) && succeeded, read); }
                catch (...) {}
                return S_OK;
            });
        if (SUCCEEDED(webview->add_NavigationCompleted(navigation.Get(), &navigation_token)))
            navigation_registered = true;

        auto failed = Callback<ICoreWebView2ProcessFailedEventHandler>(
            [weak](ICoreWebView2*, ICoreWebView2ProcessFailedEventArgs* args) noexcept {
                const auto state = weak.lock();
                auto* self = state ? state->owner.load() : nullptr;
                if (self == nullptr || !self->callbacks.process_failed) return S_OK;
                COREWEBVIEW2_PROCESS_FAILED_KIND native_kind{};
                if (args == nullptr || FAILED(args->get_ProcessFailedKind(&native_kind))) return S_OK;
                try { self->callbacks.process_failed(TranslateFailure(native_kind)); }
                catch (...) {}
                return S_OK;
            });
        if (SUCCEEDED(webview->add_ProcessFailed(failed.Get(), &process_failed_token)))
            process_failed_registered = true;

        auto accelerator = Callback<ICoreWebView2AcceleratorKeyPressedEventHandler>(
            [weak](ICoreWebView2Controller*, ICoreWebView2AcceleratorKeyPressedEventArgs* args) noexcept {
                const auto state = weak.lock();
                auto* self = state ? state->owner.load() : nullptr;
                if (self == nullptr || !self->callbacks.accelerator_key || args == nullptr) return S_OK;
                COREWEBVIEW2_KEY_EVENT_KIND kind{};
                UINT key = 0;
                INT key_lparam = 0;
                if (FAILED(args->get_KeyEventKind(&kind)) || FAILED(args->get_VirtualKey(&key)) ||
                    FAILED(args->get_KeyEventLParam(&key_lparam))) return S_OK;
                bool handled = false;
                try { handled = self->callbacks.accelerator_key(
                    {TranslateKeyMessage(kind), key, static_cast<LPARAM>(key_lparam)}); }
                catch (...) {}
                if (handled) static_cast<void>(args->put_Handled(TRUE));
                return S_OK;
            });
        if (SUCCEEDED(controller->add_AcceleratorKeyPressed(accelerator.Get(), &accelerator_token)))
            accelerator_registered = true;
    }

    void RemoveEvents() noexcept {
        if (webview && navigation_registered)
            static_cast<void>(webview->remove_NavigationCompleted(navigation_token));
        if (webview && process_failed_registered)
            static_cast<void>(webview->remove_ProcessFailed(process_failed_token));
        if (controller && accelerator_registered)
            static_cast<void>(controller->remove_AcceleratorKeyPressed(accelerator_token));
        navigation_registered = false;
        process_failed_registered = false;
        accelerator_registered = false;
    }

    WebView2Host& host;
    DWORD owner_thread = 0;
    WebView2HostState state = WebView2HostState::empty;
    WebView2InitializationResult result{};
    WebView2HostOptions options;
    WebView2HostCallbacks callbacks;
    AsyncInitialization initialization;
    std::shared_ptr<CallbackState> callback_state;
    ComPtr<ICoreWebView2Environment> environment;
    ComPtr<ICoreWebView2Controller> controller;
    ComPtr<ICoreWebView2> webview;
    EventRegistrationToken navigation_token{};
    EventRegistrationToken process_failed_token{};
    EventRegistrationToken accelerator_token{};
    bool navigation_registered = false;
    bool process_failed_registered = false;
    bool accelerator_registered = false;
};

WebView2Host::WebView2Host() : implementation_(std::make_unique<Implementation>(*this)) {}

WebView2Host::~WebView2Host() noexcept { Close(); }

bool WebView2Host::Create(HWND parent, ControlId id, RectDip bounds) {
    if (!implementation_->IsOwnerThread() || implementation_->state != WebView2HostState::empty) {
        ::SetLastError(ERROR_INVALID_STATE);
        return false;
    }
    constexpr DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
    if (!CreateNative(L"STATIC", parent, id, L"Web content", bounds, style, WS_EX_CONTROLPARENT))
        return false;
    if (::SetWindowSubclass(GetHwnd(), SubclassProcedure, kWebView2SubclassId,
                            reinterpret_cast<DWORD_PTR>(this)) == FALSE) {
        Destroy();
        return false;
    }
    implementation_->state = WebView2HostState::created;
    implementation_->result = {WebView2HostState::created, WebView2RuntimeStatus::failed, S_OK};
    return SetAccessibleName(GetHwnd(), L"Web content");
}

LRESULT CALLBACK WebView2Host::SubclassProcedure(HWND window, UINT message, WPARAM wparam,
                                                 LPARAM lparam, UINT_PTR subclass_id,
                                                 DWORD_PTR reference) noexcept {
    auto* self = reinterpret_cast<WebView2Host*>(reference);
    if (message == WM_NCDESTROY) {
        if (self != nullptr) self->Close();
        ::RemoveWindowSubclass(window, SubclassProcedure, subclass_id);
    } else if (self != nullptr) {
        if (message == WM_SIZE || message == WM_DPICHANGED_AFTERPARENT)
            static_cast<void>(self->Arrange());
        else if (message == WM_SETFOCUS)
            static_cast<void>(self->FocusContent());
        else if (message == WM_DESTROY)
            self->Close();
    }
    return ::DefSubclassProc(window, message, wparam, lparam);
}

bool WebView2Host::Initialize(WebView2HostOptions options, WebView2HostCallbacks callbacks) {
    auto& impl = *implementation_;
    if (!impl.IsOwnerThread() ||
        (impl.state != WebView2HostState::created && impl.state != WebView2HostState::failed)) {
        ::SetLastError(ERROR_INVALID_STATE);
        return false;
    }
    const WebView2RuntimeInfo runtime = QueryWebView2Runtime();
    impl.options = std::move(options);
    impl.callbacks = std::move(callbacks);
    impl.state = WebView2HostState::initializing;
    impl.result = {impl.state, runtime.status, runtime.error};
    const auto ticket = impl.initialization.Begin();
    if (!ticket) {
        impl.state = WebView2HostState::failed;
        impl.result = {impl.state, runtime.status, E_UNEXPECTED};
        return false;
    }
    if (runtime.status != WebView2RuntimeStatus::available) {
        impl.Fail(ticket, runtime.status, runtime.error);
        return true;
    }

    const std::weak_ptr<Implementation::CallbackState> weak = impl.callback_state;
    auto environment_completed = Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
        [weak, ticket](HRESULT error, ICoreWebView2Environment* environment) noexcept {
            const auto state = weak.lock();
            auto* self = state ? state->owner.load() : nullptr;
            if (self == nullptr || self->state != WebView2HostState::initializing) return S_OK;
            if (FAILED(error) || environment == nullptr) {
                self->Fail(ticket, WebView2RuntimeStatus::available,
                           FAILED(error) ? error : E_POINTER);
                return S_OK;
            }
            self->environment = environment;
            const std::weak_ptr<Implementation::CallbackState> controller_weak = weak;
            auto controller_completed = Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                [controller_weak, ticket](HRESULT controller_error,
                                          ICoreWebView2Controller* controller) noexcept {
                    const auto controller_state = controller_weak.lock();
                    auto* current = controller_state ? controller_state->owner.load() : nullptr;
                    if (current == nullptr || current->state != WebView2HostState::initializing)
                        return S_OK;
                    if (FAILED(controller_error) || controller == nullptr) {
                        current->Fail(ticket, WebView2RuntimeStatus::available,
                                      FAILED(controller_error) ? controller_error : E_POINTER);
                        return S_OK;
                    }
                    current->controller = controller;
                    const HRESULT core_result = controller->get_CoreWebView2(&current->webview);
                    if (FAILED(core_result) || !current->webview) {
                        current->Fail(ticket, WebView2RuntimeStatus::available,
                                      FAILED(core_result) ? core_result : E_POINTER);
                        return S_OK;
                    }
                    const auto dispatched = AsyncInitialization::Dispatch(ticket, [current] {
                        current->RegisterEvents();
                        current->state = WebView2HostState::ready;
                        current->result = {current->state, WebView2RuntimeStatus::available, S_OK};
                        static_cast<void>(current->host.Arrange());
                        if (!current->options.initial_uri.empty())
                            static_cast<void>(current->host.Navigate(current->options.initial_uri));
                        if (current->callbacks.initialized)
                            current->callbacks.initialized(current->result);
                    });
                    if (dispatched == AsyncDispatchStatus::delivered &&
                        current->initialization.GetResult().state == AsyncInitializationState::failed) {
                        current->RemoveEvents();
                        current->controller->Close();
                        current->controller.Reset();
                        current->webview.Reset();
                        current->state = WebView2HostState::failed;
                        current->result = {current->state, WebView2RuntimeStatus::available,
                                           E_UNEXPECTED};
                    }
                    return S_OK;
                });
            const HRESULT started = environment->CreateCoreWebView2Controller(
                self->host.GetHwnd(), controller_completed.Get());
            if (FAILED(started)) self->Fail(ticket, WebView2RuntimeStatus::available, started);
            return S_OK;
        });
    const wchar_t* folder = impl.options.user_data_folder.empty()
        ? nullptr : impl.options.user_data_folder.c_str();
    const HRESULT started = ::CreateCoreWebView2EnvironmentWithOptions(
        nullptr, folder, nullptr, environment_completed.Get());
    if (FAILED(started)) impl.Fail(ticket, WebView2RuntimeStatus::available, started);
    return true;
}

bool WebView2Host::Restart() {
    auto& impl = *implementation_;
    if (!impl.IsOwnerThread() ||
        (impl.state != WebView2HostState::ready && impl.state != WebView2HostState::failed)) {
        ::SetLastError(ERROR_INVALID_STATE);
        return false;
    }
    WebView2HostOptions options = impl.options;
    WebView2HostCallbacks callbacks = impl.callbacks;
    impl.RemoveEvents();
    if (impl.controller) impl.controller->Close();
    impl.webview.Reset();
    impl.controller.Reset();
    impl.environment.Reset();
    impl.state = WebView2HostState::created;
    impl.result = {impl.state, WebView2RuntimeStatus::failed, S_OK};
    return Initialize(std::move(options), std::move(callbacks));
}

void WebView2Host::Close() noexcept {
    if (!implementation_ || !implementation_->IsOwnerThread() ||
        implementation_->state == WebView2HostState::closed) return;
    implementation_->callback_state->owner.store(nullptr);
    implementation_->initialization.Close();
    implementation_->RemoveEvents();
    if (implementation_->controller) implementation_->controller->Close();
    implementation_->webview.Reset();
    implementation_->controller.Reset();
    implementation_->environment.Reset();
    implementation_->state = WebView2HostState::closed;
    implementation_->result.state = WebView2HostState::closed;
    implementation_->result.error = HRESULT_FROM_WIN32(ERROR_CANCELLED);
}

WebView2HostState WebView2Host::GetState() const noexcept { return implementation_->state; }
WebView2InitializationResult WebView2Host::GetInitializationResult() const noexcept {
    return implementation_->result;
}
bool WebView2Host::IsReady() const noexcept { return implementation_->state == WebView2HostState::ready; }

bool WebView2Host::Navigate(std::wstring_view uri) noexcept {
    if (!IsReady()) return false;
    const std::wstring value{uri};
    return SUCCEEDED(implementation_->webview->Navigate(value.c_str()));
}
bool WebView2Host::NavigateToString(std::wstring_view html) noexcept {
    if (!IsReady()) return false;
    const std::wstring value{html};
    return SUCCEEDED(implementation_->webview->NavigateToString(value.c_str()));
}
bool WebView2Host::GoBack() noexcept {
    return IsReady() && SUCCEEDED(implementation_->webview->GoBack());
}
bool WebView2Host::GoForward() noexcept {
    return IsReady() && SUCCEEDED(implementation_->webview->GoForward());
}
bool WebView2Host::Reload() noexcept {
    return IsReady() && SUCCEEDED(implementation_->webview->Reload());
}
bool WebView2Host::Stop() noexcept {
    return IsReady() && SUCCEEDED(implementation_->webview->Stop());
}
bool WebView2Host::FocusContent() noexcept {
    return IsReady() && SUCCEEDED(implementation_->controller->MoveFocus(
        COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC));
}
bool WebView2Host::Arrange() noexcept {
    if (!IsReady()) return GetHwnd() != nullptr;
    RECT bounds{};
    return ::GetClientRect(GetHwnd(), &bounds) != FALSE &&
           SUCCEEDED(implementation_->controller->put_Bounds(bounds));
}
void* WebView2Host::GetController() const noexcept { return implementation_->controller.Get(); }
void* WebView2Host::GetWebView() const noexcept { return implementation_->webview.Get(); }

}  // namespace mwtl
