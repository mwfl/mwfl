#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace compare_tool {

enum class Status { same, different, left_only, right_only, type_conflict, error };

struct Entry {
    std::filesystem::path relative_path;
    Status status = Status::same;
    bool left_directory = false;
    bool right_directory = false;
    std::uintmax_t left_size = 0;
    std::uintmax_t right_size = 0;
    std::wstring detail;
};

struct FolderOptions {
    bool include_same = true;
    bool verify_contents = true;
    std::wstring exclusions = L".git;.vs;build;*.obj;*.pdb";
};

struct FolderResult {
    std::vector<Entry> entries;
    std::size_t same = 0;
    std::size_t different = 0;
    std::size_t left_only = 0;
    std::size_t right_only = 0;
    std::size_t errors = 0;
    bool cancelled = false;
};

using Progress = std::function<void(std::size_t scanned, std::wstring_view current)>;

FolderResult CompareFolders(const std::filesystem::path& left,
                            const std::filesystem::path& right,
                            const FolderOptions& options,
                            const std::atomic_bool* cancel = nullptr,
                            Progress progress = {});

enum class LineKind { same, modified, left_only, right_only };

struct DiffLine {
    std::size_t left_number = 0;
    std::size_t right_number = 0;
    std::wstring left;
    std::wstring right;
    LineKind kind = LineKind::same;
};

struct TextDiff {
    std::vector<DiffLine> lines;
    std::size_t changed = 0;
    bool left_final_newline = true;
    bool right_final_newline = true;
    bool binary = false;
    std::wstring error;
};

TextDiff CompareTextFiles(const std::filesystem::path& left,
                          const std::filesystem::path& right,
                          std::size_t maximum_bytes = 32 * 1024 * 1024);

bool CopyEntryVerified(const std::filesystem::path& source_root,
                       const std::filesystem::path& destination_root,
                       const Entry& entry, std::wstring& error) noexcept;

std::wstring StatusText(Status status);

}  // namespace compare_tool
