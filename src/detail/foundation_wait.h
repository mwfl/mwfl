#pragma once

#include <algorithm>
#include <chrono>
#include <mwfl/core.h>

namespace mwfl::detail {

inline Result<CompletionStatus> WaitForDelay(Deadline deadline,
                                             std::chrono::milliseconds delay,
                                             std::stop_token stop = {}) {
    if (stop.stop_requested()) return CompletionStatus::Cancelled;
    if (deadline.Expired()) return CompletionStatus::TimedOut;
    if (!deadline.IsInfinite()) delay = (std::min)(delay, deadline.Remaining());
    delay = (std::max)(delay, std::chrono::milliseconds(1));

    KernelHandle timer(CreateWaitableTimerExW(nullptr, nullptr,
                                              CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                                              TIMER_ALL_ACCESS));
    if (!timer && GetLastError() == ERROR_INVALID_PARAMETER)
        timer.Reset(CreateWaitableTimerW(nullptr, TRUE, nullptr));
    if (!timer) return SystemError::LastWin32().WithOperation(L"Create retry timer");

    LARGE_INTEGER due{};
    due.QuadPart = -static_cast<LONGLONG>(delay.count()) * 10'000;
    if (!SetWaitableTimer(timer.Get(), &due, 0, nullptr, nullptr, FALSE))
        return SystemError::LastWin32().WithOperation(L"Set retry timer");
    auto waited = WaitForHandle(timer.Get(), deadline, stop);
    if (!waited) return waited.GetError();
    return waited.Value().status;
}

}  // namespace mwfl::detail
