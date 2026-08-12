#include <mwfl/appearance.h>

#include <oleacc.h>

#include <cstdlib>
#include <string>

int main() {
    if (mwfl::ApplyWindowAppearanceBestEffort(nullptr)) return EXIT_FAILURE;
    static_cast<void>(mwfl::IsHighContrastEnabled());
    static_cast<void>(mwfl::IsSystemDarkModePreferred());

    const HWND parent = ::CreateWindowExW(0, L"STATIC", L"appearance-test",
        WS_OVERLAPPED, 0, 0, 320, 200, nullptr, nullptr,
        ::GetModuleHandleW(nullptr), nullptr);
    if (parent == nullptr) return EXIT_FAILURE;
    struct Destroy { HWND value; ~Destroy() { ::DestroyWindow(value); } } cleanup{parent};

    const HWND child = ::CreateWindowExW(0, L"STATIC", L"", WS_CHILD,
        0, 0, 100, 20, parent, nullptr, ::GetModuleHandleW(nullptr), nullptr);
    if (child == nullptr || !mwfl::SetAccessibleName(child, L"Status summary"))
        return EXIT_FAILURE;
    wchar_t visible_text[2]{};
    if (::GetWindowTextW(child, visible_text, 2) != 0) return EXIT_FAILURE;
    IAccessible* accessible = nullptr;
    if (FAILED(::AccessibleObjectFromWindow(
            child, static_cast<DWORD>(OBJID_CLIENT), IID_IAccessible,
            reinterpret_cast<void**>(&accessible))) || accessible == nullptr) {
        return EXIT_FAILURE;
    }
    VARIANT self{};
    self.vt = VT_I4;
    self.lVal = CHILDID_SELF;
    BSTR accessible_name = nullptr;
    const HRESULT named = accessible->get_accName(self, &accessible_name);
    accessible->Release();
    const bool name_matches = SUCCEEDED(named) && accessible_name != nullptr &&
        std::wstring(accessible_name) == L"Status summary";
    ::SysFreeString(accessible_name);
    if (!name_matches) return EXIT_FAILURE;

    mwfl::AppearanceOptions options;
    options.color_mode = mwfl::ColorMode::dark;
    options.backdrop = mwfl::Backdrop::mica;
    const auto dark = mwfl::ResolveAppearance(options);
    if (!mwfl::IsHighContrastEnabled() && (!dark.IsDark() || dark.palette.text == dark.palette.window_background))
        return EXIT_FAILURE;
    if (!mwfl::ApplyWindowAppearanceBestEffort(parent, options)) return EXIT_FAILURE;
    const auto applied = mwfl::GetWindowAppearance(child);
    if (!applied.high_contrast && !applied.IsDark()) return EXIT_FAILURE;
    return EXIT_SUCCESS;
}
