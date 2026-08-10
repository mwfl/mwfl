#include <mwtl/recent_files.h>

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace mwtl {
namespace {
constexpr DWORD kSchemaVersion = 1;

class RegistryKey final {
public:
    ~RegistryKey() noexcept { if (key_) ::RegCloseKey(key_); }
    HKEY* Put() noexcept { return &key_; }
    HKEY Get() const noexcept { return key_; }
private:
    HKEY key_{};
};

SettingsStatus StatusFromError(LSTATUS error) noexcept {
    if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) return SettingsStatus::not_found;
    if (error == ERROR_ACCESS_DENIED) return SettingsStatus::access_denied;
    return SettingsStatus::io_error;
}

bool EqualPath(const std::filesystem::path& left, const std::filesystem::path& right) noexcept {
    const auto& a = left.native();
    const auto& b = right.native();
    if (a.size() > INT_MAX || b.size() > INT_MAX) return false;
    return ::CompareStringOrdinal(a.data(), static_cast<int>(a.size()),
                                  b.data(), static_cast<int>(b.size()), TRUE) == CSTR_EQUAL;
}
}  // namespace

RecentFileList::RecentFileList(std::size_t maximum_entries)
    : maximum_entries_(maximum_entries) {
    if (maximum_entries_ == 0) throw std::invalid_argument("recent file capacity must be positive");
}

bool RecentFileList::Add(std::filesystem::path path) {
    if (path.empty() || path.native().find(L'\0') != std::wstring::npos) return false;
    auto existing = std::find_if(paths_.begin(), paths_.end(),
        [&](const auto& candidate) { return EqualPath(candidate, path); });
    const bool changed = existing == paths_.end() || existing != paths_.begin();
    if (existing != paths_.end()) paths_.erase(existing);
    paths_.insert(paths_.begin(), std::move(path));
    if (paths_.size() > maximum_entries_) paths_.resize(maximum_entries_);
    return changed;
}

bool RecentFileList::Remove(const std::filesystem::path& path) {
    const auto old_size = paths_.size();
    std::erase_if(paths_, [&](const auto& candidate) { return EqualPath(candidate, path); });
    return paths_.size() != old_size;
}

RecentFilesLoadResult LoadRecentFilesFromRegistry(
    HKEY root, std::wstring_view subkey, std::size_t maximum_entries) {
    RecentFileList result{maximum_entries};
    RegistryKey key;
    const std::wstring stable_subkey{subkey};
    LSTATUS error = ::RegOpenKeyExW(root, stable_subkey.c_str(), 0, KEY_QUERY_VALUE, key.Put());
    if (error != ERROR_SUCCESS) return {
        .value = std::nullopt, .status = StatusFromError(error), .native_error = error};

    DWORD schema{};
    DWORD schema_size = sizeof(schema);
    DWORD schema_type{};
    error = ::RegQueryValueExW(key.Get(), L"SchemaVersion", nullptr, &schema_type,
                              reinterpret_cast<BYTE*>(&schema), &schema_size);
    if (error != ERROR_SUCCESS) return {
        .value = std::nullopt, .status = StatusFromError(error), .native_error = error};
    if (schema_type != REG_DWORD || schema_size != sizeof(schema) || schema != kSchemaVersion)
        return {.value = std::nullopt, .status = SettingsStatus::invalid_data,
                .native_error = ERROR_INVALID_DATA};

    DWORD type{};
    DWORD byte_count{};
    error = ::RegQueryValueExW(key.Get(), L"RecentFiles", nullptr, &type, nullptr, &byte_count);
    if (error != ERROR_SUCCESS) return {
        .value = std::nullopt, .status = StatusFromError(error), .native_error = error};
    if (type != REG_MULTI_SZ || byte_count < 2 * sizeof(wchar_t) || byte_count % sizeof(wchar_t) != 0)
        return {.value = std::nullopt, .status = SettingsStatus::invalid_data,
                .native_error = ERROR_INVALID_DATA};
    std::vector<wchar_t> data(byte_count / sizeof(wchar_t));
    error = ::RegQueryValueExW(key.Get(), L"RecentFiles", nullptr, &type,
                              reinterpret_cast<BYTE*>(data.data()), &byte_count);
    if (error != ERROR_SUCCESS) return {
        .value = std::nullopt, .status = StatusFromError(error), .native_error = error};
    if (data.size() < 2 || data.back() != L'\0' || data[data.size() - 2] != L'\0')
        return {.value = std::nullopt, .status = SettingsStatus::invalid_data,
                .native_error = ERROR_INVALID_DATA};

    std::vector<std::filesystem::path> parsed;
    std::size_t offset{};
    while (offset + 1 < data.size() && data[offset] != L'\0') {
        const auto end = std::find(data.begin() + static_cast<std::ptrdiff_t>(offset), data.end(), L'\0');
        if (end == data.end()) return {
            .value = std::nullopt, .status = SettingsStatus::invalid_data,
            .native_error = ERROR_INVALID_DATA};
        parsed.emplace_back(std::wstring(data.begin() + static_cast<std::ptrdiff_t>(offset), end));
        offset = static_cast<std::size_t>(end - data.begin()) + 1;
    }
    // Registry order is most-recent first; Add inserts at the front.
    for (auto current = parsed.rbegin(); current != parsed.rend(); ++current) result.Add(*current);
    return {.value = std::move(result), .status = SettingsStatus::success,
            .native_error = ERROR_SUCCESS};
}

SettingsWriteResult SaveRecentFilesToRegistry(
    HKEY root, std::wstring_view subkey, const RecentFileList& files) {
    RegistryKey key;
    const std::wstring stable_subkey{subkey};
    DWORD disposition{};
    LSTATUS error = ::RegCreateKeyExW(root, stable_subkey.c_str(), 0, nullptr, 0,
        KEY_SET_VALUE, nullptr, key.Put(), &disposition);
    if (error != ERROR_SUCCESS) return {.status = StatusFromError(error), .native_error = error};
    static_cast<void>(disposition);
    error = ::RegSetValueExW(key.Get(), L"SchemaVersion", 0, REG_DWORD,
                            reinterpret_cast<const BYTE*>(&kSchemaVersion), sizeof(kSchemaVersion));
    if (error != ERROR_SUCCESS) return {.status = StatusFromError(error), .native_error = error};
    std::vector<wchar_t> data;
    for (const auto& path : files.GetPaths()) {
        const auto& value = path.native();
        data.insert(data.end(), value.begin(), value.end());
        data.push_back(L'\0');
    }
    data.push_back(L'\0');
    if (files.GetPaths().empty()) data.push_back(L'\0');
    if (data.size() > (std::numeric_limits<DWORD>::max)() / sizeof(wchar_t))
        return {.status = SettingsStatus::io_error, .native_error = ERROR_BUFFER_OVERFLOW};
    error = ::RegSetValueExW(key.Get(), L"RecentFiles", 0, REG_MULTI_SZ,
        reinterpret_cast<const BYTE*>(data.data()), static_cast<DWORD>(data.size() * sizeof(wchar_t)));
    return error == ERROR_SUCCESS
        ? SettingsWriteResult{.status = SettingsStatus::success,
                              .native_error = ERROR_SUCCESS}
        : SettingsWriteResult{.status = StatusFromError(error), .native_error = error};
}

}  // namespace mwtl
