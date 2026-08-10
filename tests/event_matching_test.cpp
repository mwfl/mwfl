#include <mwfl/events.h>

#include <windows.h>
#include <commctrl.h>

namespace {

struct FakeControl {
    HWND GetHwnd() const noexcept { return window; }
    HWND window = nullptr;
};

}  // namespace

int main() {
    const auto first_window = reinterpret_cast<HWND>(static_cast<UINT_PTR>(1));
    const auto second_window = reinterpret_cast<HWND>(static_cast<UINT_PTR>(2));
    const FakeControl first{first_window};
    const FakeControl second{second_window};

    const mwfl::CommandEvent command{{42}, CBN_SELCHANGE, first_window};
    if (!command.Is({42}, CBN_SELCHANGE) ||
        !command.Is(first, CBN_SELCHANGE) || !command.IsFrom(first) ||
        command.Is(second, CBN_SELCHANGE) || command.Is(first, CBN_EDITCHANGE)) {
        return 1;
    }

    const NMHDR header{first_window, 42, NM_CLICK};
    const mwfl::NotifyEvent notify{header};
    if (!notify.Is(first, NM_CLICK) || !notify.IsFrom(first) ||
        notify.Is(second, NM_CLICK) || notify.Is(first, NM_DBLCLK)) {
        return 2;
    }
    return 0;
}
