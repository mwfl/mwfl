#include <mwfl/mwfl.h>

#include <cstdlib>
#include <cwchar>

class DpiWindow final : public mwfl::WindowBase {
public:
    void BuildUI() override { UpdateTitle(GetDpiContext()); }

    mwfl::EventResult OnDpiChanged(const mwfl::DpiChangedEvent& event) noexcept override {
        UpdateTitle(mwfl::DpiContext::FromDpi(event.dpi_x));
        return mwfl::EventResult::Propagate();
    }

private:
    void UpdateTitle(mwfl::DpiContext dpi) noexcept {
        wchar_t title[96]{};
        ::swprintf_s(title, L"DPI demo - %u DPI (%.2fx)", dpi.GetDpi(), dpi.GetScale());
        SetTitle(title);
    }

};

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
    return mwfl::RunApplication<DpiWindow>(instance, show_command);
}
