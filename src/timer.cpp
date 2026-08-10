#include <mwfl/timer.h>

#include <limits>

namespace mwfl {

UiTimer::~UiTimer() noexcept {
    Stop();
}

UiTimer::UiTimer(UiTimer&& other) noexcept
    : window_(other.window_), id_(other.id_) {
    other.window_ = nullptr;
    other.id_ = {};
}

UiTimer& UiTimer::operator=(UiTimer&& other) noexcept {
    if (this != &other) {
        Stop();
        window_ = other.window_;
        id_ = other.id_;
        other.window_ = nullptr;
        other.id_ = {};
    }
    return *this;
}

bool UiTimer::Start(
    HWND window,
    TimerId id,
    std::chrono::milliseconds interval) noexcept {
    Stop();
    if (window == nullptr || id.value == 0 || interval.count() <= 0) {
        ::SetLastError(ERROR_INVALID_PARAMETER);
        return false;
    }

    constexpr auto maximum = static_cast<long long>(
        (std::numeric_limits<UINT>::max)());
    const UINT milliseconds = static_cast<UINT>(
        interval.count() > maximum ? maximum : interval.count());
    if (::SetTimer(window, id.value, milliseconds, nullptr) == 0) {
        return false;
    }
    window_ = window;
    id_ = id;
    return true;
}

void UiTimer::Stop() noexcept {
    if (window_ != nullptr && id_ && ::IsWindow(window_) != FALSE) {
        ::KillTimer(window_, id_.value);
    }
    window_ = nullptr;
    id_ = {};
}

bool UiTimer::IsRunning() const noexcept {
    return window_ != nullptr && static_cast<bool>(id_) &&
        ::IsWindow(window_) != FALSE;
}

}  // namespace mwfl
