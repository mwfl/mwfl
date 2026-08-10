#include <mwfl/mwfl.h>

#include <cstdlib>
#include <exception>
#include <stdexcept>

class KeyboardWindow final : public mwfl::WindowBase {
public:
    void BuildUI() override {
        if (!SetTitle(L"Keyboard demo — press a key; Escape closes")) {
            throw std::runtime_error("SetTitle failed");
        }
    }

    mwfl::EventResult OnKeyDown(const mwfl::KeyEvent& event) override {
        if (event.virtual_key == VK_ESCAPE) {
            Close();
            return mwfl::EventResult::Handled();
        }
        wchar_t title[96]{};
        _snwprintf_s(title, _countof(title), _TRUNCATE,
                     L"WM_KEYDOWN virtual key: 0x%02llX",
                     static_cast<unsigned long long>(event.virtual_key));
        SetTitle(title);
        return mwfl::EventResult::Handled();
    }
};

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
    return mwfl::RunApplication<KeyboardWindow>(instance, show_command);
}
