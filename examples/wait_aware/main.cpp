#include <mwfl/mwfl.h>

#include <cstdlib>
#include <chrono>

namespace {
HWND demo_window = nullptr;
unsigned idle_ticks = 0;
}

class WaitWindow final : public mwfl::WindowBase {
public:
    void BuildUI() override {
        demo_window = GetHwnd();
        SetTitle(L"Wait-aware pump - idle ticks update this title");
    }
};

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
    using namespace std::chrono_literals;
    mwfl::WaitAwareMessagePump pump({
        .idle_interval = 500ms,
        .on_idle = [] {
        ++idle_ticks;
        wchar_t title[96]{};
        ::swprintf_s(title, L"Wait-aware pump - idle tick %u", idle_ticks);
        ::SetWindowTextW(demo_window, title);
        },
    });
    return mwfl::RunApplication<WaitWindow>(instance, show_command, pump);
}
