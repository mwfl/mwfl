#pragma once

#include <windows.h>

#include <cstddef>
#include <chrono>
#include <functional>
#include <span>
#include <vector>

namespace mwfl {

// Pre-translate hook for the UI thread's message loop. Return true to consume
// the message before translation and dispatch. Filters are non-owning: the
// filter object must outlive its registration.
class MessageFilter {
public:
    virtual ~MessageFilter() = default;
    virtual bool PreTranslateMessage(MSG& message) = 0;
};

// The UI thread's message loop and pre-translate filter chain. Application
// activates its loop on the thread it runs on; MessageLoop::Current returns
// that loop so accelerators, modeless dialogs, and property sheets can
// register filters. All members are thread-affine to the activating thread.
class MessageLoop final {
public:
    MessageLoop() noexcept = default;
    ~MessageLoop() noexcept { Deactivate(); }

    MessageLoop(const MessageLoop&) = delete;
    MessageLoop& operator=(const MessageLoop&) = delete;
    MessageLoop(MessageLoop&&) = delete;
    MessageLoop& operator=(MessageLoop&&) = delete;

    bool AddFilter(MessageFilter* filter) noexcept;
    void RemoveFilter(MessageFilter* filter) noexcept;
    // Runs registered filters in order; true when one consumed the message.
    bool PreTranslate(MSG& message);
    // Pumps until WM_QUIT and returns its exit code.
    int Run();

    // Explicit per-thread registry replacing WTL's module loop map. Activate
    // pushes this loop as the calling thread's current loop; Deactivate pops
    // it and is a no-op on any other thread or when not current.
    void Activate() noexcept;
    void Deactivate() noexcept;
    static MessageLoop* Current() noexcept;

private:
    std::vector<MessageFilter*> filters_;
    MessageLoop* previous_ = nullptr;
    bool active_ = false;
};

class MessagePump {
public:
    virtual ~MessagePump() = default;
    virtual int Run(MessageLoop& loop) noexcept = 0;
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

    int Run(MessageLoop& loop) noexcept override;

private:
    WaitAwarePumpOptions options_{};
    std::vector<HANDLE> handles_;
};

}  // namespace mwfl
