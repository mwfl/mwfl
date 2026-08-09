#pragma once

#include <windows.h>

#include <cstddef>
#include <filesystem>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace mwtl {

class RecentFileList final {
public:
    explicit RecentFileList(std::size_t maximum_entries = 10);
    std::size_t GetMaximumEntries() const noexcept { return maximum_entries_; }
    std::span<const std::filesystem::path> GetPaths() const noexcept { return paths_; }
    bool Add(std::filesystem::path path);
    bool Remove(const std::filesystem::path& path);
    void Clear() noexcept { paths_.clear(); }
private:
    std::vector<std::filesystem::path> paths_;
    std::size_t maximum_entries_;
};

enum class SettingsStatus { success, not_found, access_denied, invalid_data, io_error };

struct RecentFilesLoadResult {
    std::optional<RecentFileList> value;
    SettingsStatus status = SettingsStatus::io_error;
    LSTATUS native_error = ERROR_SUCCESS;
    bool Succeeded() const noexcept { return status == SettingsStatus::success; }
};

struct SettingsWriteResult {
    SettingsStatus status = SettingsStatus::io_error;
    LSTATUS native_error = ERROR_SUCCESS;
    bool Succeeded() const noexcept { return status == SettingsStatus::success; }
};

// Registry schema: SchemaVersion (REG_DWORD, currently 1) and RecentFiles
// (REG_MULTI_SZ). root is borrowed; these functions never close it.
RecentFilesLoadResult LoadRecentFilesFromRegistry(
    HKEY root, std::wstring_view subkey, std::size_t maximum_entries = 10);
SettingsWriteResult SaveRecentFilesToRegistry(
    HKEY root, std::wstring_view subkey, const RecentFileList& files);

}  // namespace mwtl
