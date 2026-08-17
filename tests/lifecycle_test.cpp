#include <mwfl/mwfl.h>

#include "detail/testing.h"

#include <cstdio>
#include <cstring>
#include <new>
#include <stdexcept>

namespace {

enum class TestMode {
    create_failure,
    allocation_failure,
    normal_close,
    message_failure,
    destruction_failure,
};

TestMode current_mode = TestMode::normal_close;

class TestWindow final : public mwfl::Window<TestWindow> {
public:
    void BuildUI() {
        if (current_mode == TestMode::create_failure) {
            throw std::runtime_error("injected BuildUI failure");
        }
        if (current_mode == TestMode::allocation_failure) {
            throw std::bad_alloc{};
        }

        const UINT message = current_mode == TestMode::message_failure
            ? WM_APP + 1
            : WM_CLOSE;
        if (::PostMessageW(GetHwnd(), message, 0, 0) == FALSE) {
            throw std::runtime_error("PostMessageW failed in lifecycle test");
        }
    }

    BOOL ProcessWindowMessage(HWND window, UINT message, WPARAM wparam,
                              LPARAM lparam, LRESULT& result,
                              DWORD message_map_id = 0) override {
        BOOL handled = TRUE;
        if (message == WM_APP + 1) {
            result = OnInjectedFailure(message, wparam, lparam, handled);
        } else if (message == WM_DESTROY) {
            result = OnDestroy(message, wparam, lparam, handled);
        } else {
            handled = FALSE;
        }
        return handled || mwfl::Window<TestWindow>::ProcessWindowMessage(
            window, message, wparam, lparam, result, message_map_id);
    }

private:
    LRESULT OnInjectedFailure(UINT, WPARAM, LPARAM, BOOL&) {
        if (current_mode == TestMode::message_failure) {
            throw std::runtime_error("injected message handler failure");
        }
        return 0;
    }

    LRESULT OnDestroy(UINT, WPARAM, LPARAM, BOOL& handled) {
        if (current_mode == TestMode::destruction_failure) {
            throw std::runtime_error("injected destruction failure");
        }
        handled = FALSE;
        return 0;
    }
};

int RunOnce() {
    mwfl::Application application(::GetModuleHandleW(nullptr));
    return application.Run<TestWindow>(SW_HIDE);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "expected one test mode\n");
        return 20;
    }

    bool expect_success = false;
    mwfl::detail::LifecycleSnapshot expected{1, 1, 1, 1};
    if (std::strcmp(argv[1], "normal") == 0) {
        current_mode = TestMode::normal_close;
        expect_success = true;
    } else if (std::strcmp(argv[1], "module_init_failure") == 0) {
        current_mode = TestMode::normal_close;
        expected = {1, 0, 0, 1};
    } else if (std::strcmp(argv[1], "loop_registration_failure") == 0) {
        current_mode = TestMode::normal_close;
        expected = {1, 0, 0, 1};
    } else if (std::strcmp(argv[1], "create_failure") == 0) {
        current_mode = TestMode::create_failure;
    } else if (std::strcmp(argv[1], "allocation_failure") == 0) {
        current_mode = TestMode::allocation_failure;
    } else if (std::strcmp(argv[1], "message_failure") == 0) {
        current_mode = TestMode::message_failure;
    } else if (std::strcmp(argv[1], "destruction_failure") == 0) {
        current_mode = TestMode::destruction_failure;
    } else {
        std::fprintf(stderr, "unknown test mode: %s\n", argv[1]);
        return 21;
    }

    mwfl::detail::ResetLifecycleSnapshotForTesting();
    const int run_result = RunOnce();
    if ((run_result == 0) != expect_success) {
        std::fprintf(stderr, "unexpected Application::Run result: %d\n", run_result);
        return 22;
    }

    const mwfl::detail::LifecycleSnapshot snapshot =
        mwfl::detail::GetLifecycleSnapshotForTesting();
    if (snapshot.module_initialized != expected.module_initialized ||
        snapshot.loop_registered != expected.loop_registered ||
        snapshot.loop_removed != expected.loop_removed ||
        snapshot.module_terminated != expected.module_terminated) {
        std::fprintf(
            stderr,
            "unexpected lifecycle counts: init=%d add=%d remove=%d term=%d\n",
            snapshot.module_initialized,
            snapshot.loop_registered,
            snapshot.loop_removed,
            snapshot.module_terminated);
        return 23;
    }
    return 0;
}
