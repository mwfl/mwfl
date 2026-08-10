#include <mwfl/mwfl.h>

#include <array>
#include <cstdlib>
#include <utility>

namespace {

DWORD GuiResources() noexcept {
    return ::GetGuiResources(::GetCurrentProcess(), GR_GDIOBJECTS) +
        ::GetGuiResources(::GetCurrentProcess(), GR_USEROBJECTS);
}

}  // namespace

int main() {
    const DWORD before = GuiResources();
    HWND window = ::CreateWindowExW(
        0, L"STATIC", L"mwfl menu leak fixture", WS_OVERLAPPED,
        0, 0, 100, 100, nullptr, nullptr, ::GetModuleHandleW(nullptr), nullptr);
    if (window == nullptr) return EXIT_FAILURE;
    for (int iteration = 0; iteration < 200; ++iteration) {
        mwfl::Menu menu;
        if (!menu.CreatePopup() ||
            !menu.AppendCommand(mwfl::Command({100}, L"Resource test"))) {
            return EXIT_FAILURE;
        }
        const std::array<ACCEL, 1> keys{{ACCEL{FVIRTKEY, VK_F6, 100}}};
        mwfl::AcceleratorTable accelerators;
        if (!accelerators.Create(keys)) return EXIT_FAILURE;
        mwfl::UiFont font;
        if (!font.CreateMessageFont(96)) return EXIT_FAILURE;

        mwfl::Menu bar;
        mwfl::Menu popup;
        if (!bar.Create() || !popup.CreatePopup() ||
            !popup.AppendCommand(mwfl::Command({101}, L"Attached resource test")) ||
            !bar.AppendSubmenu(std::move(popup), L"Test") ||
            !bar.AttachToWindow(window)) {
            ::DestroyWindow(window);
            return EXIT_FAILURE;
        }
    }
    ::DestroyWindow(window);
    const DWORD after = GuiResources();
    // A small allowance avoids failures caused by one-time OS font/menu caches.
    return after <= before + 4 ? EXIT_SUCCESS : EXIT_FAILURE;
}
