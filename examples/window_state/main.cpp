#include <mwfl/mwfl.h>

#include <cstdlib>
#include <exception>
#include <stdexcept>

class WindowStateWindow final : public mwfl::WindowBase {
public:
    void BuildUI() override {
        if (!SetTitle(L"Window state demo — minimize or maximize")) {
            throw std::runtime_error("SetTitle failed");
        }
    }

    mwfl::EventResult OnResize(const mwfl::ResizeEvent& event) override {
        const wchar_t* label = L"restored";
        if (event.state == mwfl::WindowSizeState::minimized) label = L"minimized";
        if (event.state == mwfl::WindowSizeState::maximized) label = L"maximized";
        wchar_t title[96]{};
        _snwprintf_s(title, _countof(title), _TRUNCATE,
                     L"Window state: %s", label);
        SetTitle(title);
        return mwfl::EventResult::Propagate();
    }
};

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
    return mwfl::RunApplication<WindowStateWindow>(instance, show_command);
}
