#include <mwfl/message_pump.h>

#include "detail/diagnostics.h"

#include <cstdint>
#include <cstdlib>
#include <algorithm>
#include <exception>
#include <limits>

namespace mwfl {

namespace {

thread_local MessageLoop* current_message_loop = nullptr;

DWORD ToNativeTimeout(std::chrono::milliseconds value) noexcept {
    if (value == (std::chrono::milliseconds::max)()) return INFINITE;
    const auto count = (std::max)(std::int64_t{0}, value.count());
    return count >= static_cast<std::int64_t>(INFINITE)
        ? INFINITE - 1
        : static_cast<DWORD>(count);
}

}  // namespace

bool MessageLoop::AddFilter(MessageFilter* filter) noexcept {
    if (filter == nullptr) return false;
    try {
        filters_.push_back(filter);
        return true;
    } catch (...) {
        return false;
    }
}

void MessageLoop::RemoveFilter(MessageFilter* filter) noexcept {
    std::erase(filters_, filter);
}

bool MessageLoop::PreTranslate(MSG& message) {
    // Index-based so a filter that unregisters during dispatch stays safe.
    for (std::size_t index = 0; index < filters_.size(); ++index) {
        MessageFilter* const filter = filters_[index];
        if (filter != nullptr && filter->PreTranslateMessage(message)) {
            return true;
        }
    }
    return false;
}

int MessageLoop::Run() {
    MSG message{};
    for (;;) {
        const BOOL result = ::GetMessageW(&message, nullptr, 0, 0);
        if (result == 0) {
            return static_cast<int>(message.wParam);
        }
        if (result == -1) {
            continue;
        }
        if (!PreTranslate(message)) {
            ::TranslateMessage(&message);
            ::DispatchMessageW(&message);
        }
    }
}

void MessageLoop::Activate() noexcept {
    if (active_) return;
    previous_ = current_message_loop;
    current_message_loop = this;
    active_ = true;
}

void MessageLoop::Deactivate() noexcept {
    if (!active_ || current_message_loop != this) return;
    current_message_loop = previous_;
    previous_ = nullptr;
    active_ = false;
}

MessageLoop* MessageLoop::Current() noexcept {
    return current_message_loop;
}

WaitAwareMessagePump::WaitAwareMessagePump(WaitAwarePumpOptions options)
    : options_(std::move(options)),
      handles_(options_.handles.begin(), options_.handles.end()) {
    options_.handles = handles_;
}

int WaitAwareMessagePump::Run(MessageLoop& loop) noexcept {
    if (options_.handles.size() > MAXIMUM_WAIT_OBJECTS - 1) {
        detail::ReportWin32(L"wait-aware message pump options", ERROR_INVALID_PARAMETER, false);
        return EXIT_FAILURE;
    }

    try {
        MSG message{};
        for (;;) {
            while (::PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE) != FALSE) {
                if (message.message == WM_QUIT) {
                    return static_cast<int>(message.wParam);
                }
                if (!loop.PreTranslate(message)) {
                    ::TranslateMessage(&message);
                    ::DispatchMessageW(&message);
                }
                if (options_.after_dispatch) options_.after_dispatch(message);
            }

            DWORD timeout = ToNativeTimeout(options_.idle_interval);
            if (options_.next_interval) {
                timeout = (std::min)(
                    timeout, ToNativeTimeout(options_.next_interval()));
            }
            const DWORD result = ::MsgWaitForMultipleObjectsEx(
                static_cast<DWORD>(options_.handles.size()),
                options_.handles.data(),
                timeout,
                QS_ALLINPUT,
                MWMO_INPUTAVAILABLE);

            if (result == WAIT_FAILED) {
                detail::ReportWin32(L"MsgWaitForMultipleObjectsEx", ::GetLastError(), false);
                return EXIT_FAILURE;
            }
            if (result == WAIT_TIMEOUT) {
                if (options_.on_idle) options_.on_idle();
                continue;
            }
            const DWORD first_handle = WAIT_OBJECT_0;
            const DWORD after_handles = first_handle +
                static_cast<DWORD>(options_.handles.size());
            if (result >= first_handle && result < after_handles) {
                if (options_.on_signal) {
                    options_.on_signal(
                        static_cast<std::size_t>(result - first_handle));
                }
                continue;
            }
            if (result != after_handles) {
                detail::ReportWin32(L"wait-aware message pump result", ERROR_INVALID_DATA, false);
                return EXIT_FAILURE;
            }
        }
    } catch (const std::exception& error) {
        detail::ReportException(L"wait-aware message pump callback", 0, error.what(), false);
    } catch (...) {
        detail::ReportUnknownException(L"wait-aware message pump callback", 0, false);
    }
    return EXIT_FAILURE;
}

}  // namespace mwfl
