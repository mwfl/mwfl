#include <mwfl/mwfl.h>

#include <cstdlib>
#include <exception>
#include <stdexcept>

namespace {
constexpr UINT kGreetingMessage = WM_APP + 42;

class NativeMessageWindow final : public mwfl::WindowBase {
public:
    void BuildUI() override {
        ::SetWindowPos(GetHwnd(), nullptr, 0, 0, 900, 560,
                       SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        if (!SetTitle(L"Native message: posting WM_APP + 42...")) {
            throw std::runtime_error("SetTitle failed");
        }
        if (::PostMessageW(GetHwnd(), kGreetingMessage, 2026, 0) == FALSE) {
            throw std::runtime_error("PostMessageW failed");
        }
    }

    mwfl::EventResult OnMessage(const mwfl::WindowMessage& message) override {
        if (message.id != kGreetingMessage) {
            return mwfl::EventResult::Propagate();
        }
        wchar_t title[128]{};
        _snwprintf_s(title, _countof(title), _TRUNCATE,
                     L"Native WM_APP received (payload: %llu)",
                     static_cast<unsigned long long>(message.wparam));
        SetTitle(title);
        return mwfl::EventResult::Handled();
    }
};
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
    return mwfl::RunApplication<NativeMessageWindow>(instance, show_command);
}
