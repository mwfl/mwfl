#include <mwfl/appearance.h>

#include <dwmapi.h>
#include <initguid.h>
#include <oleacc.h>

#include <cstring>

#include <wil/resource.h>

namespace mwfl {
namespace {

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

bool ApplyWindowAppearance(HWND window, AppearanceOptions options) noexcept {
    if (window == nullptr || ::IsWindow(window) == FALSE) return false;
    if (IsHighContrastEnabled()) options = {};
    const auto set_attribute = GetDwmSetWindowAttribute();
    if (set_attribute == nullptr) return options.color_mode == ColorMode::system &&
        options.backdrop == Backdrop::none;

    const BOOL dark = options.color_mode == ColorMode::dark ||
        (options.color_mode == ColorMode::system && IsSystemDarkModePreferred());
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
