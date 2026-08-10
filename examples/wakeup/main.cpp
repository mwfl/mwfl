#include <mwfl/mwfl.h>

#include <chrono>
#include <cstdlib>
#include <thread>

class WakeWindow final : public mwfl::WindowBase {
public:
    void BuildUI() override {
        SetTitle(L"Worker thread will wake this HWND safely");
        const mwfl::WindowWakeup wake = GetWakeup();
        worker_ = std::jthread([wake](std::stop_token stop) {
            using namespace std::chrono_literals;
            if (!stop.stop_requested()) {
                std::this_thread::sleep_for(1s);
            }
            if (!stop.stop_requested()) {
                wake.TryWake();
            }
        });
    }

    mwfl::EventResult OnWakeup() noexcept override {
        SetTitle(L"Safe cross-thread wake-up received");
        return mwfl::EventResult::Handled();
    }

private:
    std::jthread worker_;
};

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
    return mwfl::RunApplication<WakeWindow>(instance, show_command);
}
