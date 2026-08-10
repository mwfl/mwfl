#include <mwfl/mwfl.h>

#include <cstdlib>
#include <exception>
#include <stdexcept>

namespace {

constexpr UINT kShowNativeMessage = WM_APP + 1;

class WindowDemo final : public mwfl::WindowBase {
public:
    void BuildUI() override {
        if (!SetTitle(L"mwfl::Window demo — click the client area or press Esc")) {
            throw std::runtime_error("SetTitle failed");
        }
    }

    mwfl::EventResult OnLeftButtonDown(const mwfl::MouseEvent&) override {
        ::SendMessageW(GetHwnd(), kShowNativeMessage, 0, 0);
        return mwfl::EventResult::Handled();
    }

    mwfl::EventResult OnKeyDown(const mwfl::KeyEvent& event) override {
        if (event.virtual_key == VK_ESCAPE) {
            Close();
            return mwfl::EventResult::Handled();
        }
        return mwfl::EventResult::Propagate();
    }

    mwfl::EventResult OnMessage(const mwfl::WindowMessage& message) override {
        if (message.id == kShowNativeMessage) {
            SetTitle(L"Native WM_APP message received — press Esc to close");
            return mwfl::EventResult::Handled();
        }
        return mwfl::EventResult::Propagate();
    }
};

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
    return mwfl::RunApplication<WindowDemo>(instance, show_command);
}
