#include <mwfl/mwfl.h>

#include <cstdlib>

class SystemWindow final : public mwfl::WindowBase {
public:
    void BuildUI() override { SetTitle(L"Native system lifecycle messages"); }

    mwfl::EventResult OnMessage(const mwfl::WindowMessage& message) noexcept override {
        if (message.id == WM_QUERYENDSESSION) {
            return mwfl::EventResult::Handled(TRUE);
        }
        if (message.id == WM_GETOBJECT) {
            // A consumer can return UiaReturnRawElementProvider(...) here.
            return mwfl::EventResult::Propagate();
        }
        const bool lifecycle_message =
            message.id == WM_DISPLAYCHANGE || message.id == WM_SETTINGCHANGE ||
            message.id == WM_POWERBROADCAST ||
            message.id == WM_IME_STARTCOMPOSITION ||
            message.id == WM_IME_COMPOSITION || message.id == WM_ENDSESSION;
        if (!lifecycle_message) {
            return mwfl::EventResult::Propagate();
        }
        wchar_t title[96]{};
        ::swprintf_s(title, L"Native lifecycle message: 0x%04X", message.id);
        SetTitle(title);
        return mwfl::EventResult::Propagate();
    }
};

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
    return mwfl::RunApplication<SystemWindow>(
        instance,
        show_command,
        {},
        {.com_apartment = mwfl::ComApartment::sta});
}
