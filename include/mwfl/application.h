#pragma once

#include <windows.h>

#include <cstdlib>
#include <concepts>
#include <exception>

#include <mwfl/message_pump.h>
#include <mwfl/window.h>
#include <mwfl/window_options.h>

namespace mwfl {

template <typename T>
concept MainWindow = std::derived_from<T, detail::WindowMarker> &&
    requires(T& window) {
        { window.BuildUI() } -> std::same_as<void>;
    };

enum class ComApartment {
    none,
    sta,
    mta,
    // STA initialized with OleInitialize; required by OLE drag/drop APIs.
    ole_sta,
};

struct ApplicationOptions {
    ComApartment com_apartment = ComApartment::none;
};

class Application final {
public:
    explicit Application(HINSTANCE instance) noexcept;
    Application(HINSTANCE instance, ApplicationOptions options) noexcept;
    ~Application() noexcept;

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&) = delete;
    Application& operator=(Application&&) = delete;

    template <MainWindow MainWindowType>
        requires std::default_initializable<MainWindowType>
    int Run(int show_command) noexcept {
        return RunImpl<MainWindowType>(show_command, WindowOptions{}, nullptr);
    }

    template <MainWindow MainWindowType>
        requires std::default_initializable<MainWindowType>
    int Run(int show_command, const WindowOptions& options) noexcept {
        return RunImpl<MainWindowType>(show_command, options, nullptr);
    }

    template <MainWindow MainWindowType>
        requires std::default_initializable<MainWindowType>
    int Run(
        int show_command,
        const WindowOptions& options,
        MessagePump& message_pump) noexcept {
        return RunImpl<MainWindowType>(show_command, options, &message_pump);
    }

    template <MainWindow MainWindowType, typename... Arguments>
        requires (sizeof...(Arguments) > 0) &&
            std::constructible_from<MainWindowType, Arguments...>
    int Run(
        int show_command,
        const WindowOptions& options,
        Arguments&&... arguments) noexcept {
        return RunImpl<MainWindowType>(
            show_command, options, nullptr,
            std::forward<Arguments>(arguments)...);
    }

    template <MainWindow MainWindowType, typename... Arguments>
        requires (sizeof...(Arguments) > 0) &&
            std::constructible_from<MainWindowType, Arguments...>
    int Run(
        int show_command,
        const WindowOptions& options,
        MessagePump& message_pump,
        Arguments&&... arguments) noexcept {
        return RunImpl<MainWindowType>(
            show_command, options, &message_pump,
            std::forward<Arguments>(arguments)...);
    }

    HINSTANCE GetInstance() const noexcept { return instance_; }

private:
    struct EndRunGuard {
        Application* application;
        ~EndRunGuard() noexcept { application->EndRun(); }
    };

    template <MainWindow MainWindowType, typename... Arguments>
        requires std::constructible_from<MainWindowType, Arguments...>
    int RunImpl(
        int show_command,
        const WindowOptions& options,
        MessagePump* message_pump,
        Arguments&&... arguments) noexcept {
        if (!BeginRun()) {
            return EXIT_FAILURE;
        }

        const EndRunGuard cleanup{this};

        try {
            MainWindowType main_window(
                std::forward<Arguments>(arguments)...);
            main_window.ConfigureWindowOptions(options);
            // Same values as ATL's CWindow::rcDefault: the right-left width
            // computation wraps back to CW_USEDEFAULT for the default size.
            RECT bounds = options.use_default_bounds
                ? RECT{CW_USEDEFAULT, CW_USEDEFAULT, 0, 0}
                : detail::ResolveWindowBounds(options);
            const HWND window = main_window.Create(
                nullptr,
                bounds,
                options.title,
                options.style,
                options.extended_style);
            if (window == nullptr) {
                ReportWindowCreationFailure(::GetLastError());
                return EXIT_FAILURE;
            }

            main_window.ApplyNativeResources(options);

            ::ShowWindow(window, show_command);
            ::SetLastError(ERROR_SUCCESS);
            if (!::UpdateWindow(window)) {
                const DWORD error = ::GetLastError();
                if (error != ERROR_SUCCESS) {
                    ReportWindowDisplayFailure(error);
                    ::DestroyWindow(window);
                    return EXIT_FAILURE;
                }
            }

            const int loop_result = RunMessageLoop(message_pump);

            // A custom pump may stop because one of its callbacks failed before
            // the window received WM_CLOSE. Keep the stack-owned C++ window
            // object alive until its HWND is synchronously detached.
            if (main_window.IsWindow()) {
                const HWND hwnd = main_window.GetHwnd();
                if (::DestroyWindow(hwnd) == FALSE) {
                    ReportWindowDisplayFailure(::GetLastError());
                    return EXIT_FAILURE;
                }
            }

            return loop_result;
        } catch (const std::exception& error) {
            ReportRunException(error.what());
        } catch (...) {
            ReportUnknownRunException();
        }
        return EXIT_FAILURE;
    }

    bool BeginRun() noexcept;
    void EndRun() noexcept;
    int RunMessageLoop(MessagePump* message_pump);
    void ReportWindowCreationFailure(DWORD error) noexcept;
    void ReportWindowDisplayFailure(DWORD error) noexcept;
    void ReportRunException(const char* description) noexcept;
    void ReportUnknownRunException() noexcept;

    HINSTANCE instance_ = nullptr;  // Non-owning process module handle.
    ApplicationOptions options_{};
    MessageLoop message_loop_;
    bool com_initialized_ = false;
    bool ole_initialized_ = false;
    bool module_held_ = false;
    bool loop_registered_ = false;
    bool running_ = false;
};

template <MainWindow MainWindowType, typename... Arguments>
    requires std::constructible_from<MainWindowType, Arguments...>
int RunApplication(
    HINSTANCE instance,
    int show_command,
    const WindowOptions& window_options = {},
    ApplicationOptions application_options = {},
    Arguments&&... arguments) noexcept {
    Application application(instance, application_options);
    return application.Run<MainWindowType>(
        show_command, window_options,
        std::forward<Arguments>(arguments)...);
}

template <MainWindow MainWindowType, typename... Arguments>
    requires std::constructible_from<MainWindowType, Arguments...>
int RunApplication(
    HINSTANCE instance,
    int show_command,
    MessagePump& message_pump,
    const WindowOptions& window_options = {},
    ApplicationOptions application_options = {},
    Arguments&&... arguments) noexcept {
    Application application(instance, application_options);
    if constexpr (sizeof...(Arguments) == 0) {
        return application.Run<MainWindowType>(
            show_command, window_options, message_pump);
    } else {
        return application.Run<MainWindowType>(
            show_command, window_options, message_pump,
            std::forward<Arguments>(arguments)...);
    }
}

}  // namespace mwfl
