#include <mwtl/settings_store.h>

#include <wil/resource.h>

#include <algorithm>
#include <cstring>
#include <limits>
#include <utility>

namespace mwtl {
namespace {

VersionedSettingsStatus MapError(LSTATUS error) noexcept {
    if (error == ERROR_SUCCESS) return VersionedSettingsStatus::success;
    if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND)
        return VersionedSettingsStatus::not_found;
    if (error == ERROR_ACCESS_DENIED) return VersionedSettingsStatus::access_denied;
    if (error == ERROR_INVALID_DATA || error == ERROR_DATATYPE_MISMATCH)
        return VersionedSettingsStatus::malformed;
    if (error == ERROR_INVALID_PARAMETER) return VersionedSettingsStatus::invalid_argument;
    return VersionedSettingsStatus::failed;
}

bool ValidName(std::wstring_view value) noexcept {
    return !value.empty() && value.find(L'\0') == std::wstring_view::npos;
}

DWORD NativeType(SettingType type) noexcept {
    switch (type) {
        case SettingType::dword: return REG_DWORD;
        case SettingType::qword: return REG_QWORD;
        case SettingType::string: return REG_SZ;
        case SettingType::binary: return REG_BINARY;
    }
    return REG_NONE;
}

VersionedSettingsWriteResult Invalid() noexcept {
    return {VersionedSettingsStatus::invalid_argument, ERROR_INVALID_PARAMETER};
}

}  // namespace

VersionedSettingsStore::VersionedSettingsStore(HKEY root, std::wstring subkey,
                                               std::uint32_t schema_version)
    : root_(root), subkey_(std::move(subkey)), schema_version_(schema_version) {}

VersionedSettingsWriteResult VersionedSettingsStore::Save(
    std::span<const SettingValue> values) const noexcept {
    if (!root_ || !ValidName(subkey_) || schema_version_ == 0) return Invalid();
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (!ValidName(values[index].name) || values[index].name == L"SchemaVersion")
            return Invalid();
        for (std::size_t other = 0; other < index; ++other)
            if (values[other].name == values[index].name) return Invalid();
    }
    wil::unique_hkey key;
    LSTATUS error = ::RegCreateKeyExW(root_, subkey_.c_str(), 0, nullptr, 0,
                                      KEY_QUERY_VALUE | KEY_SET_VALUE, nullptr,
                                      key.put(), nullptr);
    if (error != ERROR_SUCCESS) return {MapError(error), error};
    error = ::RegDeleteValueW(key.get(), L"SchemaVersion");
    if (error != ERROR_SUCCESS && error != ERROR_FILE_NOT_FOUND)
        return {MapError(error), error};
    for (const auto& value : values) {
        const BYTE* bytes = nullptr;
        DWORD byte_count = 0;
        DWORD type = NativeType(GetSettingType(value.data));
        std::uint32_t dword = 0;
        std::uint64_t qword = 0;
        if (const auto* dword_item = std::get_if<std::uint32_t>(&value.data)) {
            dword = *dword_item; bytes = reinterpret_cast<const BYTE*>(&dword); byte_count = sizeof(dword);
        } else if (const auto* qword_item = std::get_if<std::uint64_t>(&value.data)) {
            qword = *qword_item; bytes = reinterpret_cast<const BYTE*>(&qword); byte_count = sizeof(qword);
        } else if (const auto* string_item = std::get_if<std::wstring>(&value.data)) {
            if (string_item->find(L'\0') != std::wstring::npos ||
                string_item->size() > ((std::numeric_limits<DWORD>::max)() / sizeof(wchar_t)) - 1)
                return Invalid();
            bytes = reinterpret_cast<const BYTE*>(string_item->c_str());
            byte_count = static_cast<DWORD>((string_item->size() + 1) * sizeof(wchar_t));
        } else {
            const auto& binary_item = std::get<std::vector<std::byte>>(value.data);
            if (binary_item.size() > (std::numeric_limits<DWORD>::max)()) return Invalid();
            bytes = reinterpret_cast<const BYTE*>(binary_item.data());
            byte_count = static_cast<DWORD>(binary_item.size());
        }
        error = ::RegSetValueExW(key.get(), value.name.c_str(), 0, type, bytes, byte_count);
        if (error != ERROR_SUCCESS) return {MapError(error), error};
    }
    const DWORD version = schema_version_;
    error = ::RegSetValueExW(key.get(), L"SchemaVersion", 0, REG_DWORD,
                             reinterpret_cast<const BYTE*>(&version), sizeof(version));
    return {MapError(error == ERROR_SUCCESS ? ERROR_SUCCESS : error), error};
}

SettingsResult VersionedSettingsStore::Load(
    std::span<const SettingDefinition> schema) const {
    SettingsResult result;
    if (!root_ || !ValidName(subkey_) || schema_version_ == 0) {
        result.status = VersionedSettingsStatus::invalid_argument;
        result.error = ERROR_INVALID_PARAMETER;
        return result;
    }
    for (std::size_t index = 0; index < schema.size(); ++index) {
        if (!ValidName(schema[index].name) || schema[index].maximum_bytes == 0) {
            result.status = VersionedSettingsStatus::invalid_argument; result.error = ERROR_INVALID_PARAMETER;
            return result;
        }
        for (std::size_t other = 0; other < index; ++other)
            if (schema[other].name == schema[index].name) {
                result.status = VersionedSettingsStatus::invalid_argument;
                result.error = ERROR_INVALID_PARAMETER;
                return result;
            }
    }
    wil::unique_hkey key;
    LSTATUS error = ::RegOpenKeyExW(root_, subkey_.c_str(), 0, KEY_QUERY_VALUE, key.put());
    if (error != ERROR_SUCCESS) { result.status = MapError(error); result.error = error; return result; }
    DWORD version = 0;
    DWORD version_bytes = sizeof(version);
    error = ::RegGetValueW(key.get(), nullptr, L"SchemaVersion", RRF_RT_REG_DWORD,
                           nullptr, &version, &version_bytes);
    if (error != ERROR_SUCCESS || version_bytes != sizeof(version)) {
        result.status = error == ERROR_FILE_NOT_FOUND ? VersionedSettingsStatus::malformed : MapError(error);
        result.error = error == ERROR_SUCCESS ? ERROR_INVALID_DATA : error;
        return result;
    }
    if (version != schema_version_) {
        result.status = VersionedSettingsStatus::version_mismatch;
        result.error = ERROR_REVISION_MISMATCH;
        return result;
    }
    for (const auto& definition : schema) {
        DWORD native_type = 0;
        DWORD bytes = 0;
        error = ::RegQueryValueExW(key.get(), definition.name.c_str(), nullptr,
                                   &native_type, nullptr, &bytes);
        if (error == ERROR_FILE_NOT_FOUND && !definition.required) continue;
        if (error != ERROR_SUCCESS) { result.status = MapError(error); result.error = error; return result; }
        if (native_type != NativeType(definition.type) || bytes > definition.maximum_bytes ||
            (definition.type == SettingType::dword && bytes != sizeof(DWORD)) ||
            (definition.type == SettingType::qword && bytes != sizeof(std::uint64_t)) ||
            (definition.type == SettingType::string &&
             (bytes < sizeof(wchar_t) || bytes % sizeof(wchar_t) != 0))) {
            result.status = bytes > definition.maximum_bytes ? VersionedSettingsStatus::malformed
                                                              : VersionedSettingsStatus::malformed;
            result.error = ERROR_INVALID_DATA;
            return result;
        }
        std::vector<std::byte> buffer(bytes);
        error = ::RegQueryValueExW(key.get(), definition.name.c_str(), nullptr,
                                   &native_type, reinterpret_cast<BYTE*>(buffer.data()), &bytes);
        if (error != ERROR_SUCCESS) { result.status = MapError(error); result.error = error; return result; }
        SettingValue value{definition.name, std::uint32_t{0}};
        if (definition.type == SettingType::dword) {
            std::uint32_t item = 0; std::memcpy(&item, buffer.data(), sizeof(item)); value.data = item;
        } else if (definition.type == SettingType::qword) {
            std::uint64_t item = 0; std::memcpy(&item, buffer.data(), sizeof(item)); value.data = item;
        } else if (definition.type == SettingType::string) {
            const auto* text = reinterpret_cast<const wchar_t*>(buffer.data());
            const std::size_t count = buffer.size() / sizeof(wchar_t);
            if (text[count - 1] != L'\0' ||
                std::find(text, text + count - 1, L'\0') != text + count - 1) {
                result.status = VersionedSettingsStatus::malformed; result.error = ERROR_INVALID_DATA; return result;
            }
            value.data = std::wstring(text, count - 1);
        } else {
            value.data = std::move(buffer);
        }
        result.values.push_back(std::move(value));
    }
    key.reset();
    result.status = VersionedSettingsStatus::success;
    result.error = ERROR_SUCCESS;
    return result;
}

VersionedSettingsWriteResult VersionedSettingsStore::RemoveOwned(
    std::span<const std::wstring_view> owned_values) const noexcept {
    if (!root_ || !ValidName(subkey_)) return Invalid();
    for (std::size_t index = 0; index < owned_values.size(); ++index) {
        if (!ValidName(owned_values[index]) || owned_values[index] == L"SchemaVersion")
            return Invalid();
        for (std::size_t other = 0; other < index; ++other)
            if (owned_values[other] == owned_values[index]) return Invalid();
    }
    wil::unique_hkey key;
    LSTATUS error = ::RegOpenKeyExW(root_, subkey_.c_str(), 0,
                                    KEY_QUERY_VALUE | KEY_SET_VALUE, key.put());
    if (error == ERROR_FILE_NOT_FOUND) return {VersionedSettingsStatus::not_found, error};
    if (error != ERROR_SUCCESS) return {MapError(error), error};
    for (const auto name : owned_values) {
        const std::wstring terminated(name);
        error = ::RegDeleteValueW(key.get(), terminated.c_str());
        if (error != ERROR_SUCCESS && error != ERROR_FILE_NOT_FOUND)
            return {MapError(error), error};
    }
    error = ::RegDeleteValueW(key.get(), L"SchemaVersion");
    if (error != ERROR_SUCCESS && error != ERROR_FILE_NOT_FOUND)
        return {MapError(error), error};
    DWORD subkey_count = 0;
    DWORD value_count = 0;
    error = ::RegQueryInfoKeyW(key.get(), nullptr, nullptr, nullptr, &subkey_count,
                               nullptr, nullptr, &value_count, nullptr, nullptr,
                               nullptr, nullptr);
    key.reset();
    if (error != ERROR_SUCCESS) return {MapError(error), error};
    if (subkey_count != 0 || value_count != 0)
        return {VersionedSettingsStatus::success, ERROR_SUCCESS};
    error = ::RegDeleteKeyW(root_, subkey_.c_str());
    if (error == ERROR_SUCCESS || error == ERROR_FILE_NOT_FOUND)
        return {VersionedSettingsStatus::success, ERROR_SUCCESS};
    return {MapError(error), error};
}

SettingType GetSettingType(const SettingData& data) noexcept {
    if (std::holds_alternative<std::uint32_t>(data)) return SettingType::dword;
    if (std::holds_alternative<std::uint64_t>(data)) return SettingType::qword;
    if (std::holds_alternative<std::wstring>(data)) return SettingType::string;
    return SettingType::binary;
}

const SettingValue* FindSetting(std::span<const SettingValue> values,
                                std::wstring_view name) noexcept {
    const auto found = std::find_if(values.begin(), values.end(),
        [name](const SettingValue& value) { return value.name == name; });
    return found == values.end() ? nullptr : &*found;
}

}  // namespace mwtl
