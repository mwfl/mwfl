#pragma once

#include <windows.h>

#include <chrono>

#include <mwfl/concepts.h>
#include <mwfl/events.h>

namespace mwfl {

class UiTimer final {
public:
    UiTimer() noexcept = default;
    ~UiTimer() noexcept;

    UiTimer(const UiTimer&) = delete;
    UiTimer& operator=(const UiTimer&) = delete;
    UiTimer(UiTimer&& other) noexcept;
    UiTimer& operator=(UiTimer&& other) noexcept;

    bool Start(
        HWND window,
        TimerId id,
        std::chrono::milliseconds interval) noexcept;
    template <WindowLike Window>
    bool Start(
        const Window& window,
        TimerId id,
        std::chrono::milliseconds interval) noexcept {
        return Start(window.GetHwnd(), id, interval);
    }
    void Stop() noexcept;

    bool IsRunning() const noexcept;
    TimerId GetId() const noexcept { return id_; }

private:
    HWND window_ = nullptr;  // Non-owning; valid while the timer is running.
    TimerId id_{};
};

}  // namespace mwfl
