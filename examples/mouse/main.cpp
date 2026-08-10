#include <mwfl/mwfl.h>

#include <cstdlib>
#include <exception>
#include <stdexcept>

class MouseWindow final : public mwfl::WindowBase {
public:
    void BuildUI() override {
        if (!SetTitle(L"Mouse demo — move or click in the client area")) {
            throw std::runtime_error("SetTitle failed");
        }
    }

    mwfl::EventResult OnMouseMove(const mwfl::MouseEvent& event) override {
        ShowPoint(L"WM_MOUSEMOVE", event.position);
        return mwfl::EventResult::Handled();
    }

    mwfl::EventResult OnLeftButtonDown(const mwfl::MouseEvent& event) override {
        ShowPoint(L"WM_LBUTTONDOWN", event.position);
        return mwfl::EventResult::Handled();
    }

private:
    void ShowPoint(const wchar_t* event_name, POINT position) {
        wchar_t title[128]{};
        _snwprintf_s(title, _countof(title), _TRUNCATE,
                     L"%s at client point (%ld, %ld)",
                     event_name, position.x, position.y);
        SetTitle(title);
    }
};

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
    return mwfl::RunApplication<MouseWindow>(instance, show_command);
}
