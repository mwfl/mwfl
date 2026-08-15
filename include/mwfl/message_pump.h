#pragma once

#include <windows.h>

#include <atlbase.h>
#include <atlapp.h>

#include <cstddef>
#include <chrono>
#include <functional>
#include <span>
#include <vector>

namespace mwfl {

class MessagePump {
public:
    virtual ~MessagePump() = default;
    virtual int Run(WTL::CMessageLoop& wtl_loop) noexcept = 0;
};

struct WaitAwarePumpOptions {
    std::span<const HANDLE> handles{};
    std::chrono::milliseconds idle_interval =
        (std::chrono::milliseconds::max)();
    std::function<void(const MSG&)> after_dispatch;
    std::function<void()> on_idle;
    std::function<void(std::size_t)> on_signal;
    std::function<std::chrono::milliseconds()> next_interval;
};

class WaitAwareMessagePump final : public MessagePump {
public:
    explicit WaitAwareMessagePump(WaitAwarePumpOptions options = {});

    // options_.handles aliases the owned handles_ storage, so an implicit
    // copy or move would leave the span pointing into another instance.
    WaitAwareMessagePump(const WaitAwareMessagePump&) = delete;
    WaitAwareMessagePump& operator=(const WaitAwareMessagePump&) = delete;
    WaitAwareMessagePump(WaitAwareMessagePump&&) = delete;
    WaitAwareMessagePump& operator=(WaitAwareMessagePump&&) = delete;

    int Run(WTL::CMessageLoop& wtl_loop) noexcept override;

private:
    WaitAwarePumpOptions options_{};
    std::vector<HANDLE> handles_;
};

}  // namespace mwfl
