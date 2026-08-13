#include <atomic>
#include <chrono>
#include <iostream>
#include <stop_token>
#include <string_view>
#include <thread>

using namespace std::chrono_literals;

int wmain(int argc, wchar_t** argv) {
    if (argc != 2 || std::wstring_view(argv[1]) != L"--self-test") {
        std::wcout << L"Usage: mwfl_cli_worker --self-test\n";
        return 0;
    }
    std::atomic<unsigned> iterations = 0;
    std::jthread worker([&](std::stop_token stop) {
        while (!stop.stop_requested()) {
            ++iterations;
            std::this_thread::sleep_for(1ms);
        }
    });
    while (iterations.load() < 3) std::this_thread::sleep_for(1ms);
    worker.request_stop();
    worker.join();
    std::wcout << L"worker stopped cooperatively after " << iterations.load()
               << L" iterations\n";
    return iterations.load() >= 3 ? 0 : 1;
}
