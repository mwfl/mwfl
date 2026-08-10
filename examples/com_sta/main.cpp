#include <mwfl/mwfl.h>

#include <cstdlib>
#include <objbase.h>

class ComWindow final : public mwfl::WindowBase {
public:
    void BuildUI() override {
        APTTYPE type{};
        APTTYPEQUALIFIER qualifier{};
        const bool sta = SUCCEEDED(::CoGetApartmentType(&type, &qualifier)) &&
            (type == APTTYPE_STA || type == APTTYPE_MAINSTA);
        SetTitle(sta ? L"COM STA initialized by mwfl::Application"
                     : L"Unexpected COM apartment");
    }
};

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
    return mwfl::RunApplication<ComWindow>(
        instance,
        show_command,
        {},
        {.com_apartment = mwfl::ComApartment::sta});
}
