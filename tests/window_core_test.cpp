#include <mwfl/window.h>

#include <windows.h>

namespace {

RECT HiddenBounds() noexcept { return {0, 0, 64, 64}; }

template <typename T>
HWND CreateHidden(T& window) noexcept {
    return window.Create(nullptr, HiddenBounds(), L"window core test",
                         WS_OVERLAPPED, 0);
}

class FirstWindow final : public mwfl::Window<FirstWindow> {
public:
    void BuildUI() {}
};

class SecondWindow final : public mwfl::Window<SecondWindow> {
public:
    void BuildUI() {}

    BOOL ProcessWindowMessage(HWND window, UINT message, WPARAM wparam,
                              LPARAM lparam, LRESULT& result,
                              DWORD message_map_id = 0) override {
        if (message == WM_APP + 41) {
            result = 73;
            return TRUE;
        }
        return mwfl::Window<SecondWindow>::ProcessWindowMessage(
            window, message, wparam, lparam, result, message_map_id);
    }
};

class RejectNcCreateWindow final : public mwfl::Window<RejectNcCreateWindow> {
public:
    void BuildUI() {}

    BOOL ProcessWindowMessage(HWND window, UINT message, WPARAM wparam,
                              LPARAM lparam, LRESULT& result,
                              DWORD message_map_id = 0) override {
        if (message == WM_NCCREATE) {
            result = FALSE;
            return TRUE;
        }
        return mwfl::Window<RejectNcCreateWindow>::ProcessWindowMessage(
            window, message, wparam, lparam, result, message_map_id);
    }
};

}  // namespace

int main() {
    FirstWindow first;
    const HWND first_handle = CreateHidden(first);
    if (first_handle == nullptr || !first.IsWindow()) return 1;
    if (::DestroyWindow(first_handle) == FALSE || first.GetHwnd() != nullptr) return 2;

    // A distinct Window<T> uses the same default class name. Both must share
    // the non-template trampoline instead of colliding at class registration.
    SecondWindow second;
    const HWND second_handle = CreateHidden(second);
    if (second_handle == nullptr || !second.IsWindow()) return 3;
    if (::SendMessageW(second_handle, WM_APP + 41, 0, 0) != 73) return 4;
    if (::DestroyWindow(second_handle) == FALSE || second.GetHwnd() != nullptr) return 5;

    const HWND repeated = CreateHidden(first);
    if (repeated == nullptr || ::DestroyWindow(repeated) == FALSE ||
        first.GetHwnd() != nullptr) return 6;

    RejectNcCreateWindow rejected;
    ::SetLastError(ERROR_SUCCESS);
    if (CreateHidden(rejected) != nullptr || rejected.GetHwnd() != nullptr) return 7;
    return 0;
}
