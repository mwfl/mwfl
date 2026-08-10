#pragma once

#include <windows.h>

#include <atomic>
#include <memory>

namespace mwfl {
namespace detail {

struct WindowWakeState final {
    std::atomic<HWND> window{nullptr};
};

inline UINT GetWindowWakeMessage() noexcept {
    static const UINT message = ::RegisterWindowMessageW(L"mwfl.WindowWakeup.v1");
    return message;
}

}  // namespace detail

class WindowWakeup final {
public:
    WindowWakeup() noexcept = default;

    static UINT Message() noexcept { return detail::GetWindowWakeMessage(); }

    bool TryWake() const noexcept {
        const std::shared_ptr<detail::WindowWakeState> state = state_.lock();
        if (!state) {
            return false;
        }
        const HWND window = state->window.load(std::memory_order_acquire);
        if (window == nullptr) {
            return false;
        }
        return ::PostMessageW(
                   window,
                   Message(),
                   reinterpret_cast<WPARAM>(state.get()),
                   0) != FALSE;
    }

private:
    template <typename, typename>
    friend class Window;

    explicit WindowWakeup(
        const std::shared_ptr<detail::WindowWakeState>& state) noexcept
        : state_(state) {}

    std::weak_ptr<detail::WindowWakeState> state_;
};

}  // namespace mwfl
