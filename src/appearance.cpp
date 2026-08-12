#include <mwfl/appearance.h>

#include <dwmapi.h>
#include <initguid.h>
#include <oleacc.h>
#include <uxtheme.h>

#include <cstring>
#include <iterator>

#include <wil/resource.h>

namespace mwfl {
namespace {

constexpr wchar_t kAppearanceProperty[] = L"mwfl.AppearanceState";
constexpr wchar_t kAppearanceApplyingProperty[] = L"mwfl.AppearanceApplying";

using DwmSetWindowAttributeFn = HRESULT(WINAPI*)(HWND, DWORD, LPCVOID, DWORD);

DwmSetWindowAttributeFn GetDwmSetWindowAttribute() noexcept {
    static wil::unique_hmodule module(
        ::LoadLibraryExW(L"dwmapi.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32));
    if (!module) return nullptr;
    const FARPROC address =
        ::GetProcAddress(module.get(), "DwmSetWindowAttribute");
    static_assert(sizeof(address) == sizeof(DwmSetWindowAttributeFn));
    DwmSetWindowAttributeFn function = nullptr;
    std::memcpy(&function, &address, sizeof(function));
    return function;
}

}  // namespace

bool IsHighContrastEnabled() noexcept {
    HIGHCONTRASTW contrast{};
    contrast.cbSize = sizeof(contrast);
    return ::SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast),
        &contrast, 0) != FALSE && (contrast.dwFlags & HCF_HIGHCONTRASTON) != 0;
}

bool IsSystemDarkModePreferred() noexcept {
    HKEY key = nullptr;
    DWORD value = 1;
    DWORD bytes = sizeof(value);
    if (::RegOpenKeyExW(HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
            0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) return false;
    const LSTATUS result = ::RegQueryValueExW(key, L"AppsUseLightTheme", nullptr,
        nullptr, reinterpret_cast<BYTE*>(&value), &bytes);
    ::RegCloseKey(key);
    return result == ERROR_SUCCESS && value == 0;
}

void ApplyControlTheme(HWND window, const AppearanceState& state) noexcept {
    if (!state.IsDark()) {
        static_cast<void>(::SetWindowTheme(window, L"Explorer", nullptr));
        return;
    }
    wchar_t class_name[64]{};
    static_cast<void>(::GetClassNameW(window, class_name,
                                     static_cast<int>(std::size(class_name))));
    const DWORD style = static_cast<DWORD>(::GetWindowLongPtrW(window, GWL_STYLE));
    if (_wcsicmp(class_name, L"Button") == 0 && (style & BS_TYPEMASK) == BS_GROUPBOX) {
        // The themed GroupBox paints a light title background over a dark
        // parent. Classic drawing honors WM_CTLCOLORBTN and joins the caption
        // cleanly to the border with the palette supplied by WindowBase.
        static_cast<void>(::SetWindowTheme(window, L"", L""));
    } else if (_wcsicmp(class_name, L"ComboBox") == 0 ||
               _wcsicmp(class_name, WC_COMBOBOXEXW) == 0) {
        static_cast<void>(::SetWindowTheme(window, L"DarkMode_CFD", nullptr));
    } else {
        static_cast<void>(::SetWindowTheme(window, L"DarkMode_Explorer", nullptr));
    }
}

AppearanceState ResolveAppearance(AppearanceOptions options) noexcept {
    AppearanceState state;
    state.requested_mode = options.color_mode;
    state.high_contrast = IsHighContrastEnabled();
    const bool dark = !state.high_contrast &&
        (options.color_mode == ColorMode::dark ||
         (options.color_mode == ColorMode::system && IsSystemDarkModePreferred()));
    state.effective_mode = dark ? ColorMode::dark : ColorMode::light;
    if (dark) {
        state.palette.window_background = RGB(32, 32, 32);
        state.palette.control_background = RGB(45, 45, 45);
        state.palette.text = RGB(245, 245, 245);
        state.palette.disabled_text = RGB(160, 160, 160);
        state.palette.accent = RGB(96, 205, 255);
    } else if (state.high_contrast) {
        state.palette.window_background = ::GetSysColor(COLOR_WINDOW);
        state.palette.control_background = ::GetSysColor(COLOR_WINDOW);
        state.palette.text = ::GetSysColor(COLOR_WINDOWTEXT);
        state.palette.disabled_text = ::GetSysColor(COLOR_GRAYTEXT);
        state.palette.accent = ::GetSysColor(COLOR_HIGHLIGHT);
    }
    return state;
}

AppearanceState GetWindowAppearance(HWND window) noexcept {
    for (HWND current = window; current != nullptr; current = ::GetParent(current)) {
        const ULONG_PTR encoded = reinterpret_cast<ULONG_PTR>(
            ::GetPropW(current, kAppearanceProperty));
        if (encoded == 0) continue;
        if (encoded == 3) {
            AppearanceState state = ResolveAppearance();
            state.high_contrast = true;
            state.effective_mode = ColorMode::light;
            state.palette.window_background = ::GetSysColor(COLOR_WINDOW);
            state.palette.control_background = ::GetSysColor(COLOR_WINDOW);
            state.palette.text = ::GetSysColor(COLOR_WINDOWTEXT);
            state.palette.disabled_text = ::GetSysColor(COLOR_GRAYTEXT);
            state.palette.accent = ::GetSysColor(COLOR_HIGHLIGHT);
            return state;
        }
        AppearanceOptions options;
        options.color_mode = encoded == 2 ? ColorMode::dark : ColorMode::light;
        return ResolveAppearance(options);
    }
    return ResolveAppearance();
}

bool ApplyNativeControlAppearance(HWND window, const AppearanceState& state) noexcept {
    if (window == nullptr || ::IsWindow(window) == FALSE) return false;
    const HANDLE encoded = reinterpret_cast<HANDLE>(static_cast<ULONG_PTR>(
        state.high_contrast ? 3 : (state.IsDark() ? 2 : 1)));
    static_cast<void>(::SetPropW(window, kAppearanceProperty, encoded));
    ApplyControlTheme(window, state);
    ::EnumChildWindows(window, [](HWND child, LPARAM parameter) -> BOOL {
        const auto& child_state = *reinterpret_cast<const AppearanceState*>(parameter);
        const HANDLE child_encoded = reinterpret_cast<HANDLE>(static_cast<ULONG_PTR>(
            child_state.high_contrast ? 3 : (child_state.IsDark() ? 2 : 1)));
        static_cast<void>(::SetPropW(child, kAppearanceProperty, child_encoded));
        ApplyControlTheme(child, child_state);
        return TRUE;
    }, reinterpret_cast<LPARAM>(&state));
    if (::GetMenu(window) != nullptr) static_cast<void>(::DrawMenuBar(window));
    ::RedrawWindow(window, nullptr, nullptr,
                   RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN);
    return true;
}

bool ApplyWindowAppearance(HWND window, AppearanceOptions options) noexcept {
    if (window == nullptr || ::IsWindow(window) == FALSE) return false;
    if (::GetPropW(window, kAppearanceApplyingProperty) != nullptr) return true;
    if (::SetPropW(window, kAppearanceApplyingProperty,
                   reinterpret_cast<HANDLE>(static_cast<ULONG_PTR>(1))) == FALSE) {
        return false;
    }
    const auto clear_applying = wil::scope_exit([&] {
        static_cast<void>(::RemovePropW(window, kAppearanceApplyingProperty));
    });
    if (IsHighContrastEnabled()) options = {};
    const AppearanceState state = ResolveAppearance(options);
    static_cast<void>(ApplyNativeControlAppearance(window, state));
    const auto set_attribute = GetDwmSetWindowAttribute();
    if (set_attribute == nullptr) return options.color_mode == ColorMode::system &&
        options.backdrop == Backdrop::none;

    const BOOL dark = state.IsDark();
    // DWMWA_USE_IMMERSIVE_DARK_MODE (20) is supported on current Windows 10/11.
    set_attribute(window, 20, &dark, sizeof(dark));

    const DWORD corner = options.rounded_corners ? 2u : 1u;
    set_attribute(window, 33, &corner, sizeof(corner));

    DWORD backdrop = 1;
    if (options.backdrop == Backdrop::mica) backdrop = 2;
    else if (options.backdrop == Backdrop::acrylic) backdrop = 3;
    else if (options.backdrop == Backdrop::tabbed) backdrop = 4;
    set_attribute(window, 38, &backdrop, sizeof(backdrop));
    return true;
}

bool SetAccessibleName(HWND control, const wchar_t* name) noexcept {
    if (control == nullptr || name == nullptr) return false;
    const HRESULT initialized =
        ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(initialized) && initialized != RPC_E_CHANGED_MODE) return false;
    IAccPropServices* properties = nullptr;
    const HRESULT created = ::CoCreateInstance(
        CLSID_AccPropServices, nullptr, CLSCTX_INPROC_SERVER,
        IID_IAccPropServices, reinterpret_cast<void**>(&properties));
    const HRESULT applied = SUCCEEDED(created) && properties != nullptr
        ? properties->SetHwndPropStr(
              control, static_cast<DWORD>(OBJID_CLIENT),
              static_cast<DWORD>(CHILDID_SELF), PROPID_ACC_NAME, name)
        : created;
    if (properties != nullptr) properties->Release();
    if (SUCCEEDED(initialized)) ::CoUninitialize();
    return SUCCEEDED(applied);
}

bool SetDialogDefaultButton(HWND window, UINT command_id) noexcept {
    if (window == nullptr) return false;
    return ::SendMessageW(window, DM_SETDEFID, command_id, 0) != 0;
}

}  // namespace mwfl
