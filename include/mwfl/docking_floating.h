#pragma once

#include <windows.h>

#include <exception>
#include <functional>
#include <string>

#include <mwfl/docking_workspace.h>

namespace mwfl {

enum class DockFloatingCloseAction { cancel, hide, destroy };
using DockFloatingCloseHandler = std::function<DockFloatingCloseAction()>;

struct DockFloatingWindowOptions {
    std::wstring title = L"Floating tool window";
    DockFloatingPlacement placement{};
    UINT target_dpi = 96;
    double minimum_width = 160.0;
    double minimum_height = 100.0;
    DockFloatingCloseHandler on_close;
};

enum class DockFloatingWindowStatus {
    success,
    invalid_argument,
    wrong_thread_or_process,
    stale_window,
    native_failure,
    already_attached,
    not_attached,
};

struct DockFloatingWindowResult {
    DockFloatingWindowStatus status = DockFloatingWindowStatus::invalid_argument;
    DWORD native_error = ERROR_SUCCESS;
    explicit operator bool() const noexcept {
        return status == DockFloatingWindowStatus::success;
    }
};

// Owns one auxiliary top-level HWND and borrows one application panel HWND.
// Attached content is restored to its original parent/style before explicit
// destruction. All operations and callbacks belong to the creating UI thread.
// Callback exceptions are contained and treated as close cancellation.
class DockFloatingWindow final {
public:
    DockFloatingWindow() noexcept = default;
    ~DockFloatingWindow() noexcept;
    DockFloatingWindow(const DockFloatingWindow&) = delete;
    DockFloatingWindow& operator=(const DockFloatingWindow&) = delete;

    DockFloatingWindowResult Create(
        HWND owner, DockFloatingWindowOptions options = {}) noexcept;
    DockFloatingWindowResult AttachContent(HWND content) noexcept;
    DockFloatingWindowResult DetachContent() noexcept;
    DockFloatingWindowResult SetPlacement(
        DockFloatingPlacement placement, UINT target_dpi) noexcept;
    void Show(bool activate = true) noexcept;
    void Hide() noexcept;
    void Destroy() noexcept;

    HWND GetHwnd() const noexcept { return window_; }
    HWND GetContent() const noexcept { return content_; }
    bool IsVisible() const noexcept;
    std::exception_ptr GetLastCallbackError() const noexcept {
        return last_callback_error_;
    }

private:
    static bool EnsureClass() noexcept;
    static LRESULT CALLBACK WindowProcedure(HWND window, UINT message,
                                            WPARAM wparam, LPARAM lparam) noexcept;
    bool IsCompatible(HWND window) const noexcept;
    void ArrangeContent() noexcept;
    LRESULT HandleClose() noexcept;

    HWND window_ = nullptr;
    HWND owner_ = nullptr;  // Borrowed.
    HWND content_ = nullptr;  // Borrowed.
    HWND original_parent_ = nullptr;  // Borrowed.
    LONG_PTR original_style_ = 0;
    LONG_PTR original_extended_style_ = 0;
    bool original_visible_ = false;
    DWORD thread_id_ = 0;
    DWORD process_id_ = 0;
    double minimum_width_ = 160.0;
    double minimum_height_ = 100.0;
    DockFloatingCloseHandler on_close_;
    std::exception_ptr last_callback_error_;
    bool dispatching_close_ = false;
};

}  // namespace mwfl
