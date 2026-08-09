#include "../examples/property_sheet/settings_model.h"

#include <windows.h>

#include <string>

namespace {

class TestKey final {
public:
    TestKey()
        : path_(L"Software\\mwtl\\Tests\\SettingsApplication-" +
                std::to_wstring(::GetCurrentProcessId()) + L"-" +
                std::to_wstring(::GetTickCount64())) {}
    TestKey(const TestKey&) = delete;
    TestKey& operator=(const TestKey&) = delete;
    ~TestKey() noexcept { ::RegDeleteTreeW(HKEY_CURRENT_USER, path_.c_str()); }
    const std::wstring& Get() const noexcept { return path_; }

private:
    std::wstring path_;
};

bool WriteDword(const std::wstring& path, const wchar_t* name, DWORD value) {
    HKEY key = nullptr;
    if (::RegCreateKeyExW(HKEY_CURRENT_USER, path.c_str(), 0, nullptr, 0, KEY_SET_VALUE,
                          nullptr, &key, nullptr) != ERROR_SUCCESS)
        return false;
    const LSTATUS status = ::RegSetValueExW(key, name, 0, REG_DWORD,
                                            reinterpret_cast<const BYTE*>(&value), sizeof(value));
    ::RegCloseKey(key);
    return status == ERROR_SUCCESS;
}

}  // namespace

int main() {
    using namespace settings_example;

    std::wstring embedded_null{L"Ada\0Lovelace", 12};
    if (IsValid({.display_name = L""}) || IsValid({.display_name = L" \t "}) ||
        IsValid({.display_name = embedded_null}) ||
        IsValid({.display_name = std::wstring(129, L'x')}) ||
        !IsValid({.display_name = L"Ada"}))
        return 1;

    TestKey key;
    const auto missing = LoadSettings(HKEY_CURRENT_USER, key.Get());
    if (missing.status != StoreStatus::not_found || missing.value) return 2;

    const Settings original{.display_name = L"Grace Hopper", .show_notifications = false};
    const auto saved = SaveSettings(HKEY_CURRENT_USER, key.Get(), original);
    if (saved.status == StoreStatus::access_denied) return 77;
    if (!saved.Succeeded()) return 3;
    const auto loaded = LoadSettings(HKEY_CURRENT_USER, key.Get());
    if (!loaded.Succeeded() || loaded.value != original) return 4;

    if (!WriteDword(key.Get(), L"SchemaVersion", 99) ||
        LoadSettings(HKEY_CURRENT_USER, key.Get()).status != StoreStatus::invalid_data)
        return 5;
    if (!SaveSettings(HKEY_CURRENT_USER, key.Get(), original).Succeeded() ||
        !WriteDword(key.Get(), L"ShowNotifications", 2) ||
        LoadSettings(HKEY_CURRENT_USER, key.Get()).status != StoreStatus::invalid_data)
        return 6;

    if (!SaveSettings(HKEY_CURRENT_USER, key.Get(), original).Succeeded()) return 8;
    HKEY damaged = nullptr;
    if (::RegOpenKeyExW(HKEY_CURRENT_USER, key.Get().c_str(), 0, KEY_SET_VALUE, &damaged) !=
        ERROR_SUCCESS)
        return 9;
    const LSTATUS deleted = ::RegDeleteValueW(damaged, L"DisplayName");
    ::RegCloseKey(damaged);
    if (deleted != ERROR_SUCCESS ||
        LoadSettings(HKEY_CURRENT_USER, key.Get()).status != StoreStatus::invalid_data)
        return 10;

    if (SaveSettings(nullptr, key.Get(), original).status != StoreStatus::invalid_data ||
        SaveSettings(HKEY_CURRENT_USER, L"", original).status != StoreStatus::invalid_data ||
        LoadSettings(nullptr, key.Get()).status != StoreStatus::invalid_data)
        return 7;

    return 0;
}
