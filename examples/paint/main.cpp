#include <mwfl/mwfl.h>

#include <cstdlib>
#include <exception>
#include <stdexcept>

class PaintWindow final : public mwfl::WindowBase {
public:
    void BuildUI() override {
        ::SetWindowPos(GetHwnd(), nullptr, 0, 0, 900, 560,
                       SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        if (!SetTitle(L"Native GDI paint demo")) {
            throw std::runtime_error("SetTitle failed");
        }
    }

    mwfl::EventResult OnPaint(mwfl::PaintEvent& event) override {
        const HDC dc = event.GetDC();
        if (dc != nullptr) {
            RECT client{};
            ::GetClientRect(GetHwnd(), &client);
            ::SetBkMode(dc, TRANSPARENT);
            ::SetTextColor(dc, RGB(24, 78, 119));
            ::DrawTextW(dc,
                        L"Native HWND + WM_PAINT",
                        -1, &client,
                        DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
        return mwfl::EventResult::Handled();
    }
};

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
    return mwfl::RunApplication<PaintWindow>(instance, show_command);
}
