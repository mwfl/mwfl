#include "settings_model.h"

#include <cwctype>
#include <utility>
#include <vector>

namespace settings_example {
namespace {

constexpr DWORD kSchemaVersion = 1;

class RegistryKey final {
public:
    RegistryKey() = default;
    RegistryKey(const RegistryKey&) = delete;
    RegistryKey& operator=(const RegistryKey&) = delete;
    ~RegistryKey() noexcept {
        if (key_ != nullptr) ::RegCloseKey(key_);
    }
    HKEY* Put() noexcept { return &key_; }
    HKEY Get() const noexcept { return key_; }

private:
    HKEY key_ = nullptr;
};

StoreStatus StatusFromError(LSTATUS error) noexcept {
    if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND)
        return StoreStatus::not_found;
    if (error == ERROR_ACCESS_DENIED) return StoreStatus::access_denied;
    return StoreStatus::io_error;
}

LoadResult Invalid(LSTATUS error = ERROR_INVALID_DATA) {
    return {.status = StoreStatus::invalid_data, .native_error = error};
}

LoadResult RequiredValueError(LSTATUS error) {
    if (error == ERROR_ACCESS_DENIED)
        return {.status = StoreStatus::access_denied, .native_error = error};
    if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND ||
        error == ERROR_UNSUPPORTED_TYPE || error == ERROR_MORE_DATA)
        return Invalid(error);
    return {.status = StoreStatus::io_error, .native_error = error};
}

}  // namespace

bool IsValid(const Settings& settings) noexcept {
    if (settings.display_name.empty() || settings.display_name.size() > 128) return false;
    bool has_visible_character = false;
    for (const wchar_t character : settings.display_name) {
        if (character == L'\0') return false;
        if (!std::iswspace(character)) has_visible_character = true;
    }
    return has_visible_character;
}

LoadResult LoadSettings(HKEY root, std::wstring_view subkey) {
    if (root == nullptr || subkey.empty()) return Invalid(ERROR_INVALID_PARAMETER);
    const std::wstring owned_subkey{subkey};
    RegistryKey key;
    LSTATUS error = ::RegOpenKeyExW(root, owned_subkey.c_str(), 0, KEY_QUERY_VALUE, key.Put());
    if (error != ERROR_SUCCESS)
        return {.status = StatusFromError(error), .native_error = error};

    DWORD schema = 0;
    DWORD schema_size = sizeof(schema);
    error = ::RegGetValueW(key.Get(), nullptr, L"SchemaVersion", RRF_RT_REG_DWORD, nullptr,
                          &schema, &schema_size);
    if (error != ERROR_SUCCESS) return RequiredValueError(error);
    if (schema != kSchemaVersion) return Invalid();

    DWORD name_size = 0;
    error = ::RegGetValueW(key.Get(), nullptr, L"DisplayName", RRF_RT_REG_SZ, nullptr, nullptr,
                          &name_size);
    if (error != ERROR_SUCCESS || name_size < sizeof(wchar_t) ||
        name_size > 129 * sizeof(wchar_t))
        return error == ERROR_SUCCESS ? Invalid() : RequiredValueError(error);
    std::vector<wchar_t> name(name_size / sizeof(wchar_t));
    error = ::RegGetValueW(key.Get(), nullptr, L"DisplayName", RRF_RT_REG_SZ, nullptr,
                          name.data(), &name_size);
    if (error != ERROR_SUCCESS) return RequiredValueError(error);

    DWORD notifications = 0;
    DWORD notifications_size = sizeof(notifications);
    error = ::RegGetValueW(key.Get(), nullptr, L"ShowNotifications", RRF_RT_REG_DWORD, nullptr,
                          &notifications, &notifications_size);
    if (error != ERROR_SUCCESS || notifications > 1)
        return error == ERROR_SUCCESS ? Invalid() : RequiredValueError(error);

    Settings settings{.display_name = name.data(), .show_notifications = notifications != 0};
    if (!IsValid(settings)) return Invalid();
    return {.value = std::move(settings),
            .status = StoreStatus::success,
            .native_error = ERROR_SUCCESS};
}

SaveResult SaveSettings(HKEY root, std::wstring_view subkey, const Settings& settings) {
    if (root == nullptr || subkey.empty() || !IsValid(settings))
        return {.status = StoreStatus::invalid_data, .native_error = ERROR_INVALID_PARAMETER};
    const std::wstring owned_subkey{subkey};
    RegistryKey key;
    LSTATUS error = ::RegCreateKeyExW(root, owned_subkey.c_str(), 0, nullptr, 0,
                                      KEY_SET_VALUE, nullptr, key.Put(), nullptr);
    if (error != ERROR_SUCCESS)
        return {.status = StatusFromError(error), .native_error = error};

    const DWORD notifications = settings.show_notifications ? 1u : 0u;
    const DWORD name_bytes = static_cast<DWORD>((settings.display_name.size() + 1) * sizeof(wchar_t));
    // Invalidate the old record before replacing values and publish the schema
    // marker last. A process interruption therefore cannot expose a partial
    // update as a valid current-schema record.
    error = ::RegDeleteValueW(key.Get(), L"SchemaVersion");
    if (error == ERROR_FILE_NOT_FOUND) error = ERROR_SUCCESS;
    if (error == ERROR_SUCCESS)
        error = ::RegSetValueExW(key.Get(), L"DisplayName", 0, REG_SZ,
                                 reinterpret_cast<const BYTE*>(settings.display_name.c_str()),
                                 name_bytes);
    if (error == ERROR_SUCCESS)
        error = ::RegSetValueExW(key.Get(), L"ShowNotifications", 0, REG_DWORD,
                                 reinterpret_cast<const BYTE*>(&notifications),
                                 sizeof(notifications));
    if (error == ERROR_SUCCESS)
        error = ::RegSetValueExW(key.Get(), L"SchemaVersion", 0, REG_DWORD,
                                 reinterpret_cast<const BYTE*>(&kSchemaVersion),
                                 sizeof(kSchemaVersion));
    return error == ERROR_SUCCESS
               ? SaveResult{.status = StoreStatus::success, .native_error = ERROR_SUCCESS}
               : SaveResult{.status = StatusFromError(error), .native_error = error};
}

}  // namespace settings_example
