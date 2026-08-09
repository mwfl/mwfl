#pragma once

#include <windows.h>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace mwtl {

enum class TextEncoding { utf8, utf8_bom, utf16_le, utf16_be };
enum class TextFileStatus { success, not_found, access_denied, invalid_encoding, changed, io_error };

struct FileStamp {
    std::uint64_t size{};
    std::uint64_t last_write_time{};
    std::uint64_t file_id{};
    DWORD volume_serial{};
    friend bool operator==(const FileStamp&, const FileStamp&) = default;
};

struct TextFile {
    std::wstring text;
    TextEncoding encoding = TextEncoding::utf8;
    FileStamp stamp;
};

struct TextFileReadResult {
    std::optional<TextFile> value;
    TextFileStatus status = TextFileStatus::io_error;
    DWORD native_error{};
    bool Succeeded() const noexcept { return status == TextFileStatus::success; }
};

struct TextFileWriteResult {
    std::optional<FileStamp> stamp;
    TextFileStatus status = TextFileStatus::io_error;
    DWORD native_error{};
    bool Succeeded() const noexcept { return status == TextFileStatus::success; }
};

TextFileReadResult ReadTextFile(const std::filesystem::path& path);

// Writes a sibling temporary file, flushes it, and atomically replaces the
// destination. When expected is present, replacement is rejected if the file
// has changed since it was read.
TextFileWriteResult WriteTextFileAtomic(
    const std::filesystem::path& path,
    std::wstring_view text,
    TextEncoding encoding = TextEncoding::utf8,
    std::optional<FileStamp> expected = std::nullopt);

}  // namespace mwtl
