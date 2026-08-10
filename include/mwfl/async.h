#pragma once

#include <windows.h>

#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <utility>

namespace mwfl {

// Move-only ownership for a native/COM event subscription. Revoke is invoked
// at most once. Exceptions never cross Revoke or destruction and remain
// available after an explicit Revoke call.
class UniqueSubscription final {
public:
    UniqueSubscription() noexcept = default;

    template <typename Revoke>
    explicit UniqueSubscription(Revoke&& revoke) : revoke_(std::forward<Revoke>(revoke)) {}

    ~UniqueSubscription() noexcept { Revoke(); }
    UniqueSubscription(const UniqueSubscription&) = delete;
    UniqueSubscription& operator=(const UniqueSubscription&) = delete;

    UniqueSubscription(UniqueSubscription&& other) noexcept
        : revoke_(std::move(other.revoke_)), revoke_exception_(other.revoke_exception_) {
        other.revoke_ = {};
        other.revoke_exception_ = {};
    }
    UniqueSubscription& operator=(UniqueSubscription&& other) noexcept {
        if (this == &other) return *this;
        Revoke();
        revoke_ = std::move(other.revoke_);
        revoke_exception_ = other.revoke_exception_;
        other.revoke_ = {};
        other.revoke_exception_ = {};
        return *this;
    }

    bool IsActive() const noexcept { return static_cast<bool>(revoke_); }
    bool Revoke() noexcept {
        if (!revoke_) return revoke_exception_ == nullptr;
        auto revoke = std::move(revoke_);
        revoke_ = {};
        try {
            revoke();
        } catch (...) {
            revoke_exception_ = std::current_exception();
        }
        return revoke_exception_ == nullptr;
    }
    std::exception_ptr GetRevokeException() const noexcept { return revoke_exception_; }

private:
    std::function<void()> revoke_;
    std::exception_ptr revoke_exception_;
};

enum class AsyncInitializationState { idle, pending, succeeded, failed, cancelled, closed };
enum class AsyncDispatchStatus { delivered, inactive, wrong_thread };

struct AsyncInitializationResult {
    AsyncInitializationState state = AsyncInitializationState::idle;
    HRESULT error = S_OK;
    std::exception_ptr callback_exception{};
};

namespace detail {
struct AsyncInitializationSharedState final {
    std::mutex mutex;
    DWORD owner_thread = ::GetCurrentThreadId();
    std::uint64_t generation = 0;
    AsyncInitializationResult result;
    bool dispatching = false;
};
}  // namespace detail

// Copyable callback credential returned by AsyncInitialization::Begin. Capture
// it in a native completion callback. It never keeps the owner alive.
class AsyncInitializationTicket final {
public:
    AsyncInitializationTicket() noexcept = default;
    explicit operator bool() const noexcept { return generation_ != 0 && !state_.expired(); }

private:
    friend class AsyncInitialization;
    AsyncInitializationTicket(std::weak_ptr<detail::AsyncInitializationSharedState> state,
                              std::uint64_t generation) noexcept
        : state_(std::move(state)), generation_(generation) {}
    std::weak_ptr<detail::AsyncInitializationSharedState> state_;
    std::uint64_t generation_ = 0;
};

// Owner-thread coordination for one restartable asynchronous initialization.
// Callers explicitly marshal callbacks to the creating thread: Dispatch rejects
// the wrong thread. Cancel/Close invalidate tickets, destruction expires them,
// and callback exceptions become E_UNEXPECTED failures.
class AsyncInitialization final {
public:
    AsyncInitialization()
        : state_(std::make_shared<detail::AsyncInitializationSharedState>()) {}
    ~AsyncInitialization() noexcept { Close(); }
    AsyncInitialization(const AsyncInitialization&) = delete;
    AsyncInitialization& operator=(const AsyncInitialization&) = delete;
    AsyncInitialization(AsyncInitialization&&) = delete;
    AsyncInitialization& operator=(AsyncInitialization&&) = delete;

    AsyncInitializationTicket Begin() noexcept {
        const auto state = state_;
        std::scoped_lock lock(state->mutex);
        if (::GetCurrentThreadId() != state->owner_thread ||
            state->result.state == AsyncInitializationState::closed || state->dispatching)
            return {};
        ++state->generation;
        if (state->generation == 0) ++state->generation;
        state->result = {AsyncInitializationState::pending, S_OK, {}};
        return {state, state->generation};
    }

    bool Cancel() noexcept {
        return FinishOnOwner(AsyncInitializationState::cancelled,
                             HRESULT_FROM_WIN32(ERROR_CANCELLED));
    }

    bool Fail(const AsyncInitializationTicket& ticket, HRESULT error) noexcept {
        const auto state = ticket.state_.lock();
        if (!state) return false;
        std::scoped_lock lock(state->mutex);
        if (::GetCurrentThreadId() != state->owner_thread || state->dispatching ||
            ticket.generation_ != state->generation ||
            state->result.state != AsyncInitializationState::pending)
            return false;
        state->result = {AsyncInitializationState::failed, FAILED(error) ? error : E_FAIL, {}};
        return true;
    }

    template <typename Callback>
    static AsyncDispatchStatus Dispatch(const AsyncInitializationTicket& ticket,
                                        Callback&& callback) noexcept {
        const auto state = ticket.state_.lock();
        if (!state) return AsyncDispatchStatus::inactive;
        {
            std::scoped_lock lock(state->mutex);
            if (::GetCurrentThreadId() != state->owner_thread)
                return AsyncDispatchStatus::wrong_thread;
            if (ticket.generation_ != state->generation ||
                state->result.state != AsyncInitializationState::pending || state->dispatching)
                return AsyncDispatchStatus::inactive;
            state->dispatching = true;
        }

        std::exception_ptr exception;
        try {
            std::invoke(std::forward<Callback>(callback));
        } catch (...) {
            exception = std::current_exception();
        }

        std::scoped_lock lock(state->mutex);
        state->dispatching = false;
        if (ticket.generation_ != state->generation ||
            state->result.state != AsyncInitializationState::pending)
            return AsyncDispatchStatus::inactive;
        state->result = exception
            ? AsyncInitializationResult{AsyncInitializationState::failed, E_UNEXPECTED, exception}
            : AsyncInitializationResult{AsyncInitializationState::succeeded, S_OK, {}};
        return AsyncDispatchStatus::delivered;
    }

    void Close() noexcept {
        const auto state = state_;
        std::scoped_lock lock(state->mutex);
        if (::GetCurrentThreadId() != state->owner_thread) return;
        ++state->generation;
        state->result = {AsyncInitializationState::closed,
                         HRESULT_FROM_WIN32(ERROR_CANCELLED), {}};
    }

    AsyncInitializationResult GetResult() const noexcept {
        const auto state = state_;
        std::scoped_lock lock(state->mutex);
        return state->result;
    }

private:
    bool FinishOnOwner(AsyncInitializationState finish, HRESULT error) noexcept {
        const auto state = state_;
        std::scoped_lock lock(state->mutex);
        if (::GetCurrentThreadId() != state->owner_thread ||
            state->result.state != AsyncInitializationState::pending)
            return false;
        ++state->generation;
        state->result = {finish, error, {}};
        return true;
    }
    std::shared_ptr<detail::AsyncInitializationSharedState> state_;
};

}  // namespace mwfl
