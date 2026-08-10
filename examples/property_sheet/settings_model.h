#pragma once

#include <windows.h>

#include <optional>
#include <string>
#include <string_view>

namespace settings_example {

struct Settings {
    std::wstring display_name = L"Ada Lovelace";
    bool show_notifications = true;
    friend bool operator==(const Settings&, const Settings&) = default;
};

enum class StoreStatus { success, not_found, access_denied, invalid_data, io_error };

struct LoadResult {
    std::optional<Settings> value;
    StoreStatus status = StoreStatus::io_error;
    LSTATUS native_error = ERROR_SUCCESS;
    bool Succeeded() const noexcept { return status == StoreStatus::success; }
};

struct SaveResult {
    StoreStatus status = StoreStatus::io_error;
    LSTATUS native_error = ERROR_SUCCESS;
    bool Succeeded() const noexcept { return status == StoreStatus::success; }
};

// root is borrowed. The schema is versioned and rejects unknown versions or
// malformed value types instead of silently replacing user data.
LoadResult LoadSettings(HKEY root, std::wstring_view subkey);
SaveResult SaveSettings(HKEY root, std::wstring_view subkey, const Settings& settings);

bool IsValid(const Settings& settings) noexcept;

}  // namespace settings_example
