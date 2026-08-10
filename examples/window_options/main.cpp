#include <mwfl/mwfl.h>

#include <cstdlib>

using mwfl::operator""_dip;

struct DemoClassTraits {
    static const wchar_t* GetClassName() noexcept { return L"mwfl.WindowOptionsDemo"; }
    static UINT GetClassStyle() noexcept { return CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS; }
    static HICON GetIcon() noexcept { return nullptr; }
    static HICON GetSmallIcon() noexcept { return nullptr; }
    static HCURSOR GetCursor() noexcept { return ::LoadCursorW(nullptr, IDC_ARROW); }
    static HBRUSH GetBackground() noexcept { return reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1); }
};

class OptionsWindow final : public mwfl::Window<OptionsWindow, DemoClassTraits> {
public:
    void BuildUI() { SetTitle(L"Configurable class, style and DIP client size"); }
};

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
    return mwfl::RunApplication<OptionsWindow>(
        instance,
        show_command,
        {
            .title = L"Window options demo",
            .initial_bounds = {{0.0_dip, 0.0_dip}, {800.0_dip, 500.0_dip}},
            .use_default_bounds = false,
            .center_in_work_area = true,
            .bounds_are_client_size = true,
        });
}
