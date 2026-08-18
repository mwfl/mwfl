#include <mwfl/core.h>
#include <chrono>
#include <iostream>
#include <stop_token>
#include <string_view>
using namespace std::chrono_literals;
int wmain(int argc, wchar_t** argv) {
    if (argc != 2 || std::wstring_view(argv[1]) != L"--self-test") {
        std::wcout << L"Usage: mwfl_core_foundation --self-test\n"; return 0;
    }
    auto wide = mwfl::Utf8ToWide("MWFL \xE2\x9C\x93");
    if (!wide) return 1;
    auto utf8 = mwfl::WideToUtf8(wide.Value());
    if (!utf8 || utf8.Value() != "MWFL \xE2\x9C\x93") return 2;
    mwfl::KernelHandle event(CreateEventW(nullptr, TRUE, TRUE, nullptr));
    if (!event) return 3;
    auto signaled = mwfl::WaitForHandle(event.Get(), mwfl::Deadline::After(100ms));
    if (!signaled || signaled.Value().status != mwfl::CompletionStatus::Completed) return 4;
    std::stop_source source; source.request_stop();
    auto cancelled = mwfl::WaitForHandle(event.Get(), mwfl::Deadline::After(100ms),
                                         source.get_token());
    if (!cancelled || cancelled.Value().status != mwfl::CompletionStatus::Cancelled) return 5;
    std::wcout << L"core ownership, Unicode, wait, and cancellation passed\n";
    return 0;
}
