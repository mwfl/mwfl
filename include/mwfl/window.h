#pragma once

#include <windows.h>

#include <cstdlib>
#include <concepts>
#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include <mwfl/dpi.h>
#include <mwfl/error.h>
#include <mwfl/events.h>
#include <mwfl/layout.h>
#include <mwfl/wakeup.h>
#include <mwfl/window_options.h>
#include <mwfl/detail/message_decode.h>
#include <mwfl/detail/window_appearance.h>
#include <mwfl/detail/window_support.h>

namespace mwfl {

namespace detail {
struct WindowMarker {};

class WindowCore {
protected:
    HWND CreateNativeWindow(const WNDCLASSEXW& descriptor, HWND parent,
                            const RECT& bounds, const wchar_t* title,
                            DWORD style, DWORD extended_style,
                            HMENU menu, void* object) noexcept {
        if (window_ != nullptr) {
            ::SetLastError(ERROR_INVALID_STATE);
            return nullptr;
        }

        WNDCLASSEXW existing{sizeof(existing)};
        if (::GetClassInfoExW(descriptor.hInstance, descriptor.lpszClassName,
                              &existing) == FALSE) {
            WNDCLASSEXW registered = descriptor;
            registered.lpfnWndProc = &InitialWindowProc;
            if (::RegisterClassExW(&registered) == 0) return nullptr;
        } else if (existing.lpfnWndProc != &InitialWindowProc) {
            ::SetLastError(ERROR_CLASS_ALREADY_EXISTS);
            return nullptr;
        }

        const int width = bounds.left == CW_USEDEFAULT
            ? CW_USEDEFAULT : bounds.right - bounds.left;
        const int height = bounds.top == CW_USEDEFAULT
            ? CW_USEDEFAULT : bounds.bottom - bounds.top;
        return ::CreateWindowExW(
            extended_style, descriptor.lpszClassName, title, style,
            bounds.left, bounds.top, width, height, parent, menu,
            descriptor.hInstance, object);
    }

    HWND window_ = nullptr;

private:
    virtual LRESULT DispatchNativeWindowMessage(
        HWND window, UINT message, WPARAM wparam, LPARAM lparam) noexcept = 0;

    static LRESULT CALLBACK InitialWindowProc(
        HWND window, UINT message, WPARAM wparam, LPARAM lparam) noexcept {
        WindowCore* self = nullptr;
        if (message == WM_NCCREATE) {
            const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lparam);
            self = static_cast<WindowCore*>(create->lpCreateParams);
            self->window_ = window;
            ::SetWindowLongPtrW(window, GWLP_USERDATA,
                                reinterpret_cast<LONG_PTR>(self));
        } else {
            self = reinterpret_cast<WindowCore*>(
                ::GetWindowLongPtrW(window, GWLP_USERDATA));
        }
        if (self == nullptr) {
            return ::DefWindowProcW(window, message, wparam, lparam);
        }

        const LRESULT result = self->DispatchNativeWindowMessage(
            window, message, wparam, lparam);
        if (message == WM_NCDESTROY) {
            ::SetWindowLongPtrW(window, GWLP_USERDATA, 0);
            self->window_ = nullptr;
        }
        return result;
    }
};
}

template <typename T, typename ClassTraits = DefaultWindowClassTraits>
class Window : public detail::WindowCore, public detail::WindowMarker {
public:
    Window()
        : accelerator_filter_(this),
          wake_state_(std::make_shared<detail::WindowWakeState>()) {}
    ~Window() = default;

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) = delete;
    Window& operator=(Window&&) = delete;

    HWND Create(HWND parent, const RECT& bounds, const wchar_t* title,
                DWORD style, DWORD extended_style = 0, HMENU menu = nullptr) noexcept {
        const WNDCLASSEXW descriptor{
            sizeof(WNDCLASSEXW), ClassTraits::GetClassStyle(), nullptr,
            0, 0, ::GetModuleHandleW(nullptr), ClassTraits::GetIcon(),
            ClassTraits::GetCursor(), ClassTraits::GetBackground(), nullptr,
            ClassTraits::GetClassName(), ClassTraits::GetSmallIcon()};
        return CreateNativeWindow(descriptor, parent, bounds, title, style,
                                  extended_style, menu,
                                  static_cast<detail::WindowCore*>(this));
    }

    HWND GetHwnd() const noexcept { return window_; }
    HACCEL GetAccelerators() const noexcept { return m_hAccel; }

    bool IsWindow() const noexcept {
        return window_ != nullptr && ::IsWindow(window_) != FALSE;
    }

    bool SetTitle(const wchar_t* title) noexcept {
        return window_ != nullptr && ::SetWindowTextW(window_, title) != FALSE;
    }

    bool SetTitle(std::wstring_view title) {
        const std::wstring terminated{title};
        return SetTitle(terminated.c_str());
    }

    LRESULT Close() noexcept {
        return window_ != nullptr
            ? ::SendMessageW(window_, WM_CLOSE, 0, 0)
            : 0;
    }

    DpiContext GetDpiContext() const noexcept {
        return DpiContext::FromWindow(GetHwnd());
    }

    WindowWakeup GetWakeup() const noexcept { return WindowWakeup(wake_state_); }

    const AppearanceState& GetAppearanceState() const noexcept {
        return appearance_.GetState();
    }

    bool SetAppearance(AppearanceOptions options) noexcept {
        return appearance_.Set(GetHwnd(), options);
    }

    void SetAccelerators(HACCEL accelerators) noexcept {
        m_hAccel = accelerators;  // Non-owning; the table must outlive the window.
        MessageLoop* loop = MessageLoop::Current();
        if (loop == nullptr) return;
        if (accelerators != nullptr && !accelerator_filter_registered_) {
            accelerator_filter_registered_ =
                loop->AddFilter(&accelerator_filter_);
        } else if (accelerators == nullptr && accelerator_filter_registered_) {
            loop->RemoveFilter(&accelerator_filter_);
            accelerator_filter_registered_ = false;
        }
    }

    void SetLayout(LayoutNode root) {
        owned_layout_.emplace();
        owned_layout_->SetRoot(std::move(root));
        layout_ = &*owned_layout_;
        if (IsWindow() && !layout_->Arrange(GetHwnd())) {
            layout_ = nullptr;
            owned_layout_.reset();
            throw Error(ERROR_GEN_FAILURE, "arrange the owned window layout");
        }
    }

    void ClearLayout() noexcept {
        layout_ = nullptr;
        owned_layout_.reset();
    }

    void ConfigureWindowOptions(const WindowOptions& options) noexcept {
        apply_suggested_dpi_rect_ = options.apply_suggested_dpi_rect;
        quit_on_destroy_ = options.quit_on_destroy;
        system_message_font_.Configure(options.use_system_message_font);
        appearance_.Configure(options.appearance);
    }
    void ApplyNativeResources(const WindowOptions& options) noexcept {
        const HWND window = GetHwnd();
        if (window == nullptr) {
            return;
        }
        if (options.icon != nullptr) {
            ::SendMessageW(window, WM_SETICON, ICON_BIG,
                           reinterpret_cast<LPARAM>(options.icon));
        }
        if (options.small_icon != nullptr) {
            ::SendMessageW(window, WM_SETICON, ICON_SMALL,
                           reinterpret_cast<LPARAM>(options.small_icon));
        }
        if (options.cursor != nullptr) {
            ::SetClassLongPtrW(
                window, GCLP_HCURSOR, reinterpret_cast<LONG_PTR>(options.cursor));
        }
        if (options.background != nullptr) {
            ::SetClassLongPtrW(
                window, GCLP_HBRBACKGROUND,
                reinterpret_cast<LONG_PTR>(options.background));
        }
        static_cast<void>(appearance_.Reapply(window));
    }

    virtual BOOL ProcessWindowMessage(
        HWND window,
        UINT message,
        WPARAM wparam,
        LPARAM lparam,
        LRESULT& result,
        DWORD message_map_id = 0) {
        static_cast<void>(window);
        if (message_map_id == 0 &&
            DispatchModernMessage(message, wparam, lparam, result)) {
            return TRUE;
        }
        return FALSE;
    }

private:
    LRESULT DispatchNativeWindowMessage(
        HWND window, UINT message, WPARAM wparam, LPARAM lparam) noexcept override {
        return SafeWindowProc(window, message, wparam, lparam);
    }
    template <typename Callback>
    static bool InvokeModernHandler(Callback&& callback, LRESULT& result) {
        using Return = std::invoke_result_t<Callback>;
        if constexpr (std::same_as<Return, void>) {
            std::invoke(std::forward<Callback>(callback));
            result = 0;
            return true;
        } else {
            static_assert(
                std::same_as<Return, EventResult>,
                "mwfl convention handlers must return void or mwfl::EventResult");
            const EventResult reply =
                std::invoke(std::forward<Callback>(callback));
            result = reply.result;
            return reply.handled;
        }
    }

    bool DispatchModernMessage(
        UINT message,
        WPARAM wparam,
        LPARAM lparam,
        LRESULT& result) {
        T& target = *static_cast<T*>(this);

        if (message == WM_THEMECHANGED || message == WM_SETTINGCHANGE ||
            message == WM_SYSCOLORCHANGE) {
            if (appearance_.IsApplying()) return false;
            static_cast<void>(appearance_.Reapply(GetHwnd()));
            if constexpr (requires(T& value, const AppearanceState& state) {
                              value.OnAppearanceChanged(state);
                          }) {
                if (InvokeModernHandler(
                        [&target, this] {
                            return target.OnAppearanceChanged(appearance_.GetState());
                        }, result)) {
                    return true;
                }
            }
        }
        if (appearance_.HandleColorMessage(GetHwnd(), message, wparam, result)) return true;

        if (message == WM_CLOSE) {
            if constexpr (requires(T& value) { value.OnClose(); }) {
                return InvokeModernHandler(
                    [&target] { return target.OnClose(); }, result);
            }
        }
        if (message == WM_KEYDOWN) {
            if constexpr (requires(T& value, const KeyEvent& event) {
                              value.OnKeyDown(event);
                          }) {
                const KeyEvent event = detail::DecodeKeyEvent(wparam, lparam);
                return InvokeModernHandler(
                    [&target, &event] { return target.OnKeyDown(event); }, result);
            }
        }
        if (message == WM_MOUSEMOVE || message == WM_LBUTTONDOWN) {
            const MouseEvent event = detail::DecodeMouseEvent(wparam, lparam);
            if (message == WM_MOUSEMOVE) {
                if constexpr (requires(T& value, const MouseEvent& input) {
                                  value.OnMouseMove(input);
                              }) {
                    return InvokeModernHandler(
                        [&target, &event] { return target.OnMouseMove(event); },
                        result);
                }
            } else {
                if constexpr (requires(T& value, const MouseEvent& input) {
                                  value.OnLeftButtonDown(input);
                              }) {
                    return InvokeModernHandler(
                        [&target, &event] {
                            return target.OnLeftButtonDown(event);
                        },
                        result);
                }
            }
        }
        if (message == WM_HSCROLL || message == WM_VSCROLL) {
            if constexpr (requires(T& value, const ScrollEvent& event) {
                              value.OnScroll(event);
                          }) {
                const ScrollEvent event =
                    detail::DecodeScrollEvent(message, wparam, lparam);
                return InvokeModernHandler(
                    [&target, &event] { return target.OnScroll(event); },
                    result);
            }
        }
        if (message == WM_SIZE) {
            if (layout_ != nullptr && wparam != SIZE_MINIMIZED) {
                static_cast<void>(layout_->Arrange(GetHwnd()));
            }
            if constexpr (requires(T& value, const ResizeEvent& event) {
                              value.OnResize(event);
                          }) {
                const ResizeEvent event =
                    detail::DecodeResizeEvent(wparam, lparam);
                return InvokeModernHandler(
                    [&target, &event] { return target.OnResize(event); }, result);
            }
        }
        if (message == WM_TIMER) {
            if constexpr (requires(T& value, TimerId id) { value.OnTimer(id); }) {
                return InvokeModernHandler(
                    [&target, wparam] { return target.OnTimer(TimerId{wparam}); },
                    result);
            }
        }
        if (message == WM_DPICHANGED) {
            system_message_font_.Apply(GetHwnd(), LOWORD(wparam));
            if constexpr (requires(T& value, const DpiChangedEvent& event) {
                              value.OnDpiChanged(event);
                          }) {
                const RECT empty{};
                const RECT& suggested = lparam != 0
                    ? *reinterpret_cast<const RECT*>(lparam)
                    : empty;
                const DpiChangedEvent event{
                    .dpi_x = LOWORD(wparam),
                    .dpi_y = HIWORD(wparam),
                    .suggested_bounds = suggested,
                };
                return InvokeModernHandler(
                    [&target, &event] { return target.OnDpiChanged(event); },
                    result);
            }
        }
        if (message == WM_COMMAND) {
            if constexpr (requires(T& value, const CommandEvent& event) {
                              value.OnCommand(event);
                          }) {
                const CommandEvent event =
                    detail::DecodeCommandEvent(wparam, lparam);
                return InvokeModernHandler(
                    [&target, &event] { return target.OnCommand(event); }, result);
            }
        }
        if (message == WM_NOTIFY && lparam != 0) {
            if constexpr (requires(T& value, const NotifyEvent& event) {
                              value.OnNotify(event);
                          }) {
                const NotifyEvent event{*reinterpret_cast<const NMHDR*>(lparam)};
                return InvokeModernHandler(
                    [&target, &event] { return target.OnNotify(event); }, result);
            }
        }
        if (message == WM_GETMINMAXINFO && lparam != 0) {
            if constexpr (requires(T& value, MinMaxInfoEvent event) {
                              value.OnMinMaxInfo(event);
                          }) {
                MinMaxInfoEvent event{
                    *reinterpret_cast<MINMAXINFO*>(lparam)};
                return InvokeModernHandler(
                    [&target, &event] { return target.OnMinMaxInfo(event); },
                    result);
            }
        }
        if (message == WM_PAINT) {
            if constexpr (requires(T& value, PaintEvent& event) {
                              value.OnPaint(event);
                          }) {
                PaintEvent event{GetHwnd()};
                return InvokeModernHandler(
                    [&target, &event] { return target.OnPaint(event); }, result);
            }
        }
        if (message == WindowWakeup::Message()) {
            if constexpr (requires(T& value) { value.OnWakeup(); }) {
                return InvokeModernHandler(
                    [&target] { return target.OnWakeup(); }, result);
            }
        }
        if (message != WM_CREATE && message != WM_DESTROY &&
            message != WM_NCDESTROY) {
            if constexpr (requires(T& value, const WindowMessage& event) {
                              value.OnMessage(event);
                          }) {
                const WindowMessage event =
                    detail::DecodeWindowMessage(message, wparam, lparam);
                return InvokeModernHandler(
                    [&target, &event] { return target.OnMessage(event); },
                    result);
            }
        }
        return false;
    }

    static const wchar_t* StageFor(UINT message, bool creation_complete) noexcept {
        if (!creation_complete) {
            return L"window creation";
        }
        if (message == WM_DESTROY || message == WM_NCDESTROY) {
            return L"window destruction";
        }
        return L"window message dispatch";
    }

    static LRESULT CALLBACK SafeWindowProc(
        HWND window,
        UINT message,
        WPARAM wparam,
        LPARAM lparam) noexcept {
        auto* self = reinterpret_cast<Window<T, ClassTraits>*>(
            ::GetWindowLongPtrW(window, GWLP_USERDATA));
        if (self == nullptr) return ::DefWindowProcW(window, message, wparam, lparam);
        const bool creation_message = !self->creation_complete_;
        const wchar_t* const stage = StageFor(message, self->creation_complete_);

        try {
            if (message == WindowWakeup::Message() &&
                wparam != reinterpret_cast<WPARAM>(self->wake_state_.get())) {
                return 0;
            }

            if (message == WM_CLOSE && self->recovery_requested_) {
                const HWND recovery_window = self->window_;
                return recovery_window != nullptr &&
                    ::DestroyWindow(recovery_window) != FALSE ? 0 : -1;
            }

            if (message == WM_CREATE) {
                self->wake_state_->window.store(self->window_, std::memory_order_release);
                self->system_message_font_.Apply(self->window_,
                    DpiContext::FromWindow(self->window_).GetDpi());
                static_cast<T*>(self)->BuildUI();
            }
            if (message == WM_DPICHANGED && self->apply_suggested_dpi_rect_ &&
                lparam != 0) {
                const RECT* suggested = reinterpret_cast<const RECT*>(lparam);
                ::SetWindowPos(
                    self->window_,
                    nullptr,
                    suggested->left,
                    suggested->top,
                    suggested->right - suggested->left,
                    suggested->bottom - suggested->top,
                    SWP_NOZORDER | SWP_NOACTIVATE);
            }

            // WM_NCDESTROY member cleanup stays before base dispatch: an
            // overridden OnFinalMessage may destroy this object inside it.
            if (message == WM_NCDESTROY) {
                if (self->accelerator_filter_registered_) {
                    if (MessageLoop* loop = MessageLoop::Current(); loop != nullptr) {
                        loop->RemoveFilter(&self->accelerator_filter_);
                    }
                    self->accelerator_filter_registered_ = false;
                }
                self->wake_state_->window.store(nullptr, std::memory_order_release);
                self->system_message_font_.Detach(self->window_);
            }

            LRESULT result = 0;
            const BOOL handled = self->ProcessWindowMessage(
                window, message, wparam, lparam, result);
            if (!handled) {
                result = ::DefWindowProcW(window, message, wparam, lparam);
            }

            if (message == WM_CREATE && result != -1) {
                self->creation_complete_ = true;
            }
            if (message == WM_DESTROY) {
                if (self->quit_on_destroy_) ::PostQuitMessage(self->exit_code_);
            }
            return result;
        } catch (const std::exception& error) {
            detail::ReportException(stage, message, error.what(), creation_message);
        } catch (...) {
            detail::ReportUnknownException(stage, message, creation_message);
        }

        return self->RecoverFromDispatchFailure(message, wparam, lparam, creation_message);
    }

    LRESULT RecoverFromDispatchFailure(
        UINT message,
        WPARAM wparam,
        LPARAM lparam,
        bool creation_message) noexcept {
        exit_code_ = EXIT_FAILURE;
        ::SetLastError(ERROR_UNHANDLED_EXCEPTION);

        if (creation_message) {
            return message == WM_CREATE ? -1 : 0;
        }

        if (message == WM_NCDESTROY) {
            wake_state_->window.store(nullptr, std::memory_order_release);
            const HWND window = window_;
            const LRESULT result = window != nullptr
                ? ::DefWindowProcW(window, message, wparam, lparam)
                : 0;
            if (quit_on_destroy_) ::PostQuitMessage(exit_code_);
            return result;
        }

        if (message == WM_DESTROY) {
            if (quit_on_destroy_) ::PostQuitMessage(exit_code_);
            return 0;
        }

        recovery_requested_ = true;
        if (quit_on_destroy_ && (window_ == nullptr ||
            ::PostMessageW(window_, WM_CLOSE, 0, 0) == FALSE)) {
            ::PostQuitMessage(exit_code_);
        }
        return 0;
    }

    detail::AcceleratorFilter<Window> accelerator_filter_;
    HACCEL m_hAccel = nullptr;
    bool creation_complete_ = false;
    bool accelerator_filter_registered_ = false;
    bool recovery_requested_ = false;
    bool apply_suggested_dpi_rect_ = true, quit_on_destroy_ = true;
    detail::SystemMessageFont system_message_font_;
    detail::WindowAppearance appearance_;
    std::optional<LayoutHost> owned_layout_;
    LayoutHost* layout_ = nullptr;  // Non-owning.
    int exit_code_ = EXIT_SUCCESS;
    std::shared_ptr<detail::WindowWakeState> wake_state_;
};

// Concise inheritance for applications using the virtual event surface.
class WindowBase : public Window<WindowBase> {
public:
    virtual ~WindowBase() = default;

    virtual void BuildUI() = 0;

    virtual EventResult OnClose() { return EventResult::Propagate(); }
    virtual EventResult OnKeyDown(const KeyEvent&) { return EventResult::Propagate(); }
    virtual EventResult OnMouseMove(const MouseEvent&) { return EventResult::Propagate(); }
    virtual EventResult OnLeftButtonDown(const MouseEvent&) { return EventResult::Propagate(); }
    virtual EventResult OnScroll(const ScrollEvent&) { return EventResult::Propagate(); }
    virtual EventResult OnResize(const ResizeEvent&) { return EventResult::Propagate(); }
    virtual EventResult OnTimer(TimerId) { return EventResult::Propagate(); }
    virtual EventResult OnDpiChanged(const DpiChangedEvent&) { return EventResult::Propagate(); }
    // Called after MWFL has reapplied the requested appearance to the window,
    // native descendants, and attached menu following a system theme change.
    // Use the palette for application-owned drawing resources.
    virtual EventResult OnAppearanceChanged(const AppearanceState&) {
        return EventResult::Propagate();
    }
    virtual EventResult OnCommand(const CommandEvent&) { return EventResult::Propagate(); }
    virtual EventResult OnNotify(const NotifyEvent&) { return EventResult::Propagate(); }
    virtual EventResult OnMinMaxInfo(MinMaxInfoEvent) { return EventResult::Propagate(); }
    virtual EventResult OnPaint(PaintEvent&) { return EventResult::Propagate(); }
    virtual EventResult OnWakeup() { return EventResult::Propagate(); }
    // Never sees a message a typed handler above receives, even on Propagate.
    virtual EventResult OnMessage(const WindowMessage&) { return EventResult::Propagate(); }
};

}  // namespace mwfl
