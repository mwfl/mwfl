#include <mwfl/core.h>
#include <cassert>
#include <chrono>
#include <stop_token>
using namespace std::chrono_literals;
int main() {
    auto invalid = mwfl::Utf8ToWide(std::string_view("\xC3\x28", 2)); assert(!invalid);
    auto text = mwfl::Utf8ToWide("hello \xE2\x9C\x93"); assert(text && text.Value() == L"hello \u2713");
    auto round_trip = mwfl::WideToUtf8(text.Value()); assert(round_trip && round_trip.Value() == "hello \xE2\x9C\x93");
    mwfl::KernelHandle event(CreateEventW(nullptr, TRUE, FALSE, nullptr)); assert(event);
    auto timeout = mwfl::WaitForHandle(event.Get(), 0ms); assert(timeout && timeout.Value().status == mwfl::WaitStatus::Timeout);
    std::stop_source source; source.request_stop();
    auto cancelled = mwfl::WaitForHandle(event.Get(), 1s, source.get_token()); assert(cancelled && cancelled.Value().status == mwfl::WaitStatus::Cancelled);
    HANDLE raw = event.Release(); assert(raw != nullptr && !event); CloseHandle(raw);
}
