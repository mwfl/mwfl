#include <mwfl/mwfl.h>

#include <cstdlib>
#include <exception>
#include <stdexcept>

class ResizeWindow final : public mwfl::WindowBase {
public:
    void BuildUI() override {
        if (!SetTitle(L"Resize demo — resize this window")) {
            throw std::runtime_error("SetTitle failed");
        }
    }

    mwfl::EventResult OnResize(const mwfl::ResizeEvent& event) override {
        wchar_t title[128]{};
        _snwprintf_s(title, _countof(title), _TRUNCATE,
                     L"WM_SIZE: %ld × %ld",
                     event.client_size.cx, event.client_size.cy);
        SetTitle(title);
        return mwfl::EventResult::Propagate();
    }
};

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
    return mwfl::RunApplication<ResizeWindow>(instance, show_command);
}
