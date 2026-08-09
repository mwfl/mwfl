#pragma once

#include <windows.h>

#include <exception>
#include <functional>
#include <memory>
#include <string>

#include <mwtl/dpi.h>
#include <mwtl/layout.h>

namespace mwtl {

enum class DialogStatus {
    accepted,
    cancelled,
    failed,
};

struct DialogResult {
    DialogStatus status = DialogStatus::failed;
    INT_PTR command_id = 0;
    DWORD error = ERROR_SUCCESS;
    std::exception_ptr callback_exception;

    explicit operator bool() const noexcept { return status == DialogStatus::accepted; }
};

struct DialogCallbacks {
    std::function<bool(HWND)> initialize;
    std::function<bool(HWND, WORD, WORD)> command;
    // Return false to reject a user-initiated WM_CLOSE. Programmatic Close()
    // remains deterministic and cannot be vetoed.
    std::function<bool(HWND)> close;
    std::function<void()> destroyed;
};

struct DialogOptions {
    HWND owner = nullptr;  // Borrowed; native modal calls disable it automatically.
    std::wstring title;
    SizeDip initial_client_size{Dip{480.0f}, Dip{320.0f}};
    bool resizable = true;
    // A modeless dialog normally leaves its owner enabled. Set this for a
    // pseudo-modal workflow; the owner is restored exactly once on teardown.
    bool disable_owner_for_modeless = false;
    DialogCallbacks callbacks;
};

// Move-only owner for a native dialog and its callback/layout state. Except for
// Accept(), Cancel(), and destruction (which post a close request), operations
// belong to the creating thread. The creating thread must continue pumping
// messages until a cross-thread close request is processed. Modal display uses
// the native nested loop and returns a structured accepted/cancelled/failed
// result. GetHwnd() is the borrowed native escape hatch.
class Dialog final {
   public:
    explicit Dialog(DialogOptions options);
    ~Dialog() noexcept;

    Dialog(const Dialog&) = delete;
    Dialog& operator=(const Dialog&) = delete;
    Dialog(Dialog&& other) noexcept;
    Dialog& operator=(Dialog&& other) noexcept;

    bool SetLayout(LayoutNode root);
    void ClearLayout() noexcept;

    DialogResult ShowModal() noexcept;
    bool CreateModeless() noexcept;
    DialogResult GetResult() const noexcept;

    HWND GetHwnd() const noexcept;
    bool IsWindow() const noexcept;
    bool IsOwnerThread() const noexcept;
    bool IsModeless() const noexcept;

    // These two close immediately on the owner thread and post when called from
    // another thread. A successful post is not synchronous completion.
    bool Accept(INT_PTR command_id = IDOK) noexcept;
    bool Cancel(INT_PTR command_id = IDCANCEL) noexcept;
    void Close() noexcept;

   private:
    struct State;
    static INT_PTR CALLBACK Procedure(HWND window, UINT message, WPARAM wparam,
                                      LPARAM lparam) noexcept;
    std::shared_ptr<State> state_;
};

}  // namespace mwtl
