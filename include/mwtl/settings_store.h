#pragma once

#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace mwtl {

enum class SettingType { dword, qword, string, binary };
enum class SettingsStatus {
    success,
    not_found,
    version_mismatch,
    malformed,
    access_denied,
    invalid_argument,
    failed,
};

using SettingData = std::variant<std::uint32_t, std::uint64_t, std::wstring,
                                 std::vector<std::byte>>;

struct SettingValue {
    std::wstring name;
    SettingData data;
};

struct SettingDefinition {
    std::wstring name;
    SettingType type = SettingType::string;
    std::size_t maximum_bytes = 1024 * 1024;
    bool required = true;
};

struct SettingsResult {
    std::vector<SettingValue> values;
    SettingsStatus status = SettingsStatus::failed;
    LSTATUS error = ERROR_GEN_FAILURE;
    explicit operator bool() const noexcept { return status == SettingsStatus::success; }
};

struct SettingsWriteResult {
    SettingsStatus status = SettingsStatus::failed;
    LSTATUS error = ERROR_GEN_FAILURE;
    explicit operator bool() const noexcept { return status == SettingsStatus::success; }
};

// Focused per-user registry store. root is borrowed. Save removes the commit
// marker, writes only supplied values, then writes SchemaVersion last. Load
// never calls application callbacks while a registry key is open.
class VersionedSettingsStore final {
public:
    VersionedSettingsStore(HKEY root, std::wstring subkey,
                           std::uint32_t schema_version);

    SettingsWriteResult Save(std::span<const SettingValue> values) const noexcept;
    SettingsResult Load(std::span<const SettingDefinition> schema) const;
    // Removes only names in owned_values plus SchemaVersion. The key itself is
    // deleted only when empty; unrelated values/subkeys are never traversed.
    SettingsWriteResult RemoveOwned(std::span<const std::wstring_view> owned_values) const noexcept;

    HKEY GetRoot() const noexcept { return root_; }
    const std::wstring& GetSubkey() const noexcept { return subkey_; }
    std::uint32_t GetSchemaVersion() const noexcept { return schema_version_; }

private:
    HKEY root_ = nullptr;
    std::wstring subkey_;
    std::uint32_t schema_version_ = 0;
};

SettingType GetSettingType(const SettingData& data) noexcept;
const SettingValue* FindSetting(std::span<const SettingValue> values,
                                std::wstring_view name) noexcept;

}  // namespace mwtl
