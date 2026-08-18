#include <chrono>
#include <mwfl/core.h>
#include <stop_token>
#include <thread>
using namespace std::chrono_literals;
int main() {
    auto invalid = mwfl::Utf8ToWide(std::string_view("\xC3\x28", 2));
    if (invalid) return 1;
    auto text = mwfl::Utf8ToWide("hello \xE2\x9C\x93");
    if (!text || text.Value() != L"hello \u2713") return 2;
    auto round_trip = mwfl::WideToUtf8(text.Value());
    if (!round_trip || round_trip.Value() != "hello \xE2\x9C\x93") return 3;
    mwfl::KernelHandle event(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    if (!event) return 4;
    auto timeout = mwfl::WaitForHandle(event.Get(), mwfl::Deadline::After(0ms));
    if (!timeout || timeout.Value().status != mwfl::CompletionStatus::TimedOut) return 5;
    std::stop_source source;
    source.request_stop();
    auto cancelled =
        mwfl::WaitForHandle(event.Get(), mwfl::Deadline::After(1s), source.get_token());
    if (!cancelled || cancelled.Value().status != mwfl::CompletionStatus::Cancelled) return 6;
    mwfl::SystemError contextual =
        mwfl::SystemError::FromWin32(ERROR_ACCESS_DENIED).WithOperation(L"open worker");
    if (contextual.code != ERROR_ACCESS_DENIED ||
        !contextual.Message().starts_with(L"open worker: "))
        return 7;
    SetEvent(event.Get());
    auto completed_first =
        mwfl::WaitForHandle(event.Get(), mwfl::Deadline::After(1s), source.get_token());
    if (!completed_first || completed_first.Value().status != mwfl::CompletionStatus::Cancelled)
        return 8;

    mwfl::KernelHandle race_begin(CreateEventW(nullptr, FALSE, FALSE, nullptr));
    mwfl::KernelHandle race_done(CreateEventW(nullptr, FALSE, FALSE, nullptr));
    if (!race_begin || !race_done) return 9;
    std::stop_source* active_source = nullptr;
    std::jthread canceller([&] {
        for (;;) {
            if (WaitForSingleObject(race_begin.Get(), INFINITE) != WAIT_OBJECT_0) return;
            if (!active_source) {
                SetEvent(race_done.Get());
                return;
            }
            active_source->request_stop();
            SetEvent(race_done.Get());
        }
    });
    for (int iteration = 0; iteration < 10'000; ++iteration) {
        ResetEvent(event.Get());
        std::stop_source race_source;
        active_source = &race_source;
        SetEvent(race_begin.Get());
        SetEvent(event.Get());
        auto raced = mwfl::WaitForHandle(event.Get(), mwfl::Deadline::After(1s),
                                         race_source.get_token());
        if (!raced || (raced.Value().status != mwfl::CompletionStatus::Completed &&
                       raced.Value().status != mwfl::CompletionStatus::Cancelled))
            return 10;
        if (WaitForSingleObject(race_done.Get(), INFINITE) != WAIT_OBJECT_0) return 11;
    }
    active_source = nullptr;
    SetEvent(race_begin.Get());
    if (WaitForSingleObject(race_done.Get(), INFINITE) != WAIT_OBJECT_0) return 12;

    HANDLE raw = event.Release();
    if (!raw || event) return 13;
    CloseHandle(raw);
    return 0;
}
