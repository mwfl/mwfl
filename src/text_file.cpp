#include <mwtl/text_file.h>

#include <atomic>
#include <cstddef>
#include <limits>
#include <utility>
#include <vector>

namespace mwtl {
namespace {

class FileHandle final {
public:
    explicit FileHandle(HANDLE handle = INVALID_HANDLE_VALUE) noexcept : handle_(handle) {}
    ~FileHandle() noexcept { if (handle_ != INVALID_HANDLE_VALUE) ::CloseHandle(handle_); }
    FileHandle(const FileHandle&) = delete;
    FileHandle& operator=(const FileHandle&) = delete;
    HANDLE Get() const noexcept { return handle_; }
    bool Valid() const noexcept { return handle_ != INVALID_HANDLE_VALUE; }
    void Reset() noexcept {
        if (handle_ != INVALID_HANDLE_VALUE) ::CloseHandle(handle_);
        handle_ = INVALID_HANDLE_VALUE;
    }
private:
    HANDLE handle_;
};

TextFileStatus StatusFromError(DWORD error) noexcept {
    if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) return TextFileStatus::not_found;
    if (error == ERROR_ACCESS_DENIED || error == ERROR_SHARING_VIOLATION) return TextFileStatus::access_denied;
    return TextFileStatus::io_error;
}

std::uint64_t ToUint64(FILETIME value) noexcept {
    ULARGE_INTEGER converted{};
    converted.LowPart = value.dwLowDateTime;
    converted.HighPart = value.dwHighDateTime;
    return converted.QuadPart;
}

bool QueryStamp(HANDLE file, FileStamp& stamp, DWORD& error) noexcept {
    BY_HANDLE_FILE_INFORMATION info{};
    if (!::GetFileInformationByHandle(file, &info)) {
        error = ::GetLastError();
        return false;
    }
    ULARGE_INTEGER size{};
    size.LowPart = info.nFileSizeLow;
    size.HighPart = info.nFileSizeHigh;
    ULARGE_INTEGER file_id{};
    file_id.LowPart = info.nFileIndexLow;
    file_id.HighPart = info.nFileIndexHigh;
    stamp = {size.QuadPart, ToUint64(info.ftLastWriteTime),
             file_id.QuadPart, info.dwVolumeSerialNumber};
    return true;
}

bool IsValidUtf16(std::wstring_view text) noexcept {
    for (std::size_t index = 0; index < text.size(); ++index) {
        const wchar_t value = text[index];
        if (value >= 0xD800 && value <= 0xDBFF) {
            if (++index >= text.size() || text[index] < 0xDC00 || text[index] > 0xDFFF) return false;
        } else if (value >= 0xDC00 && value <= 0xDFFF) {
            return false;
        }
    }
    return true;
}

bool DecodeUtf8(const std::byte* data, std::size_t size, std::wstring& output) {
    if (size > static_cast<std::size_t>((std::numeric_limits<int>::max)())) return false;
    if (size == 0) { output.clear(); return true; }
    const int input_size = static_cast<int>(size);
    const auto* input = reinterpret_cast<const char*>(data);
    const int required = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, input, input_size, nullptr, 0);
    if (required <= 0) return false;
    output.resize(static_cast<std::size_t>(required));
    return ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, input, input_size,
                                 output.data(), required) == required;
}

bool DecodeUtf16(const std::byte* data, std::size_t size, bool big_endian, std::wstring& output) {
    if ((size % 2) != 0) return false;
    output.resize(size / 2);
    for (std::size_t index = 0; index < output.size(); ++index) {
        const auto first = std::to_integer<unsigned char>(data[index * 2]);
        const auto second = std::to_integer<unsigned char>(data[index * 2 + 1]);
        output[index] = static_cast<wchar_t>(big_endian ? (first << 8) | second
                                                       : first | (second << 8));
    }
    return IsValidUtf16(output);
}

bool Encode(std::wstring_view text, TextEncoding encoding, std::vector<std::byte>& bytes) {
    if (!IsValidUtf16(text)) return false;
    if (encoding == TextEncoding::utf8 || encoding == TextEncoding::utf8_bom) {
        if (text.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)())) return false;
        const int count = static_cast<int>(text.size());
        const int required = count == 0 ? 0 : ::WideCharToMultiByte(
            CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), count, nullptr, 0, nullptr, nullptr);
        if (count != 0 && required <= 0) return false;
        const std::size_t prefix = encoding == TextEncoding::utf8_bom ? 3 : 0;
        bytes.resize(prefix + static_cast<std::size_t>(required));
        if (prefix) { bytes[0] = std::byte{0xEF}; bytes[1] = std::byte{0xBB}; bytes[2] = std::byte{0xBF}; }
        return required == 0 || ::WideCharToMultiByte(
            CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), count,
            reinterpret_cast<char*>(bytes.data() + prefix), required, nullptr, nullptr) == required;
    }
    bytes.resize(2 + text.size() * 2);
    const bool big = encoding == TextEncoding::utf16_be;
    bytes[0] = big ? std::byte{0xFE} : std::byte{0xFF};
    bytes[1] = big ? std::byte{0xFF} : std::byte{0xFE};
    for (std::size_t index = 0; index < text.size(); ++index) {
        const auto value = static_cast<unsigned>(text[index]);
        bytes[2 + index * 2] = static_cast<std::byte>(big ? value >> 8 : value & 0xFF);
        bytes[3 + index * 2] = static_cast<std::byte>(big ? value & 0xFF : value >> 8);
    }
    return true;
}

bool ReadAll(HANDLE file, std::uint64_t size, std::vector<std::byte>& bytes, DWORD& error) {
    if (size > static_cast<std::uint64_t>((std::numeric_limits<std::size_t>::max)())) {
        error = ERROR_FILE_TOO_LARGE;
        return false;
    }
    bytes.resize(static_cast<std::size_t>(size));
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        const DWORD request = static_cast<DWORD>((std::min)(bytes.size() - offset,
            static_cast<std::size_t>((std::numeric_limits<DWORD>::max)())));
        DWORD read{};
        if (!::ReadFile(file, bytes.data() + offset, request, &read, nullptr) || read == 0) {
            error = ::GetLastError();
            if (error == ERROR_SUCCESS) error = ERROR_HANDLE_EOF;
            return false;
        }
        offset += read;
    }
    return true;
}

bool WriteAll(HANDLE file, const std::vector<std::byte>& bytes, DWORD& error) noexcept {
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        const DWORD request = static_cast<DWORD>((std::min)(bytes.size() - offset,
            static_cast<std::size_t>((std::numeric_limits<DWORD>::max)())));
        DWORD written{};
        if (!::WriteFile(file, bytes.data() + offset, request, &written, nullptr) || written == 0) {
            error = ::GetLastError();
            return false;
        }
        offset += written;
    }
    return true;
}

std::filesystem::path TemporarySibling(const std::filesystem::path& path) {
    static std::atomic<unsigned long> sequence{};
    auto temporary = path;
    temporary += L".mwtl-tmp-" + std::to_wstring(::GetCurrentProcessId()) + L"-" +
                 std::to_wstring(sequence.fetch_add(1, std::memory_order_relaxed));
    return temporary;
}

}  // namespace

TextFileReadResult ReadTextFile(const std::filesystem::path& path) {
    FileHandle file{::CreateFileW(path.c_str(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr)};
    if (!file.Valid()) {
        const DWORD error = ::GetLastError();
        return {.status = StatusFromError(error), .native_error = error};
    }
    FileStamp before{};
    DWORD error{};
    if (!QueryStamp(file.Get(), before, error)) return {.status = StatusFromError(error), .native_error = error};
    std::vector<std::byte> bytes;
    if (!ReadAll(file.Get(), before.size, bytes, error)) return {.status = StatusFromError(error), .native_error = error};
    FileStamp after{};
    if (!QueryStamp(file.Get(), after, error)) return {.status = StatusFromError(error), .native_error = error};
    if (before != after) return {.status = TextFileStatus::changed};

    TextFile result;
    result.stamp = after;
    const auto starts = [&](unsigned a, unsigned b) {
        return bytes.size() >= 2 && bytes[0] == static_cast<std::byte>(a) &&
               bytes[1] == static_cast<std::byte>(b);
    };
    bool decoded{};
    if (starts(0xFF, 0xFE)) {
        result.encoding = TextEncoding::utf16_le;
        decoded = DecodeUtf16(bytes.data() + 2, bytes.size() - 2, false, result.text);
    } else if (starts(0xFE, 0xFF)) {
        result.encoding = TextEncoding::utf16_be;
        decoded = DecodeUtf16(bytes.data() + 2, bytes.size() - 2, true, result.text);
    } else {
        const bool bom = bytes.size() >= 3 && bytes[0] == std::byte{0xEF} &&
                         bytes[1] == std::byte{0xBB} && bytes[2] == std::byte{0xBF};
        result.encoding = bom ? TextEncoding::utf8_bom : TextEncoding::utf8;
        decoded = DecodeUtf8(bytes.data() + (bom ? 3 : 0), bytes.size() - (bom ? 3 : 0), result.text);
    }
    if (!decoded) return {.status = TextFileStatus::invalid_encoding, .native_error = ERROR_NO_UNICODE_TRANSLATION};
    return {.value = std::move(result), .status = TextFileStatus::success};
}

TextFileWriteResult WriteTextFileAtomic(const std::filesystem::path& path,
                                        std::wstring_view text, TextEncoding encoding,
                                        std::optional<FileStamp> expected) {
    if (path.empty()) return {.status = TextFileStatus::io_error, .native_error = ERROR_INVALID_NAME};
    if (expected) {
        FileHandle current{::CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr)};
        DWORD error{};
        FileStamp actual{};
        if (!current.Valid()) {
            error = ::GetLastError();
            return {.status = error == ERROR_FILE_NOT_FOUND ? TextFileStatus::changed : StatusFromError(error),
                    .native_error = error};
        }
        if (!QueryStamp(current.Get(), actual, error)) return {.status = StatusFromError(error), .native_error = error};
        if (actual != *expected) return {.status = TextFileStatus::changed};
    }
    std::vector<std::byte> bytes;
    if (!Encode(text, encoding, bytes)) {
        return {.status = TextFileStatus::invalid_encoding, .native_error = ERROR_NO_UNICODE_TRANSLATION};
    }
    const auto temporary = TemporarySibling(path);
    FileHandle output{::CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr)};
    if (!output.Valid()) {
        const DWORD error = ::GetLastError();
        return {.status = StatusFromError(error), .native_error = error};
    }
    DWORD error{};
    if (!WriteAll(output.Get(), bytes, error) || !::FlushFileBuffers(output.Get())) {
        if (error == ERROR_SUCCESS) error = ::GetLastError();
        output.Reset();
        ::DeleteFileW(temporary.c_str());
        return {.status = StatusFromError(error), .native_error = error};
    }
    // Close the temporary before replacement.
    output.Reset();
    if (expected) {
        FileHandle current{::CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr)};
        FileStamp actual{};
        if (!current.Valid() || !QueryStamp(current.Get(), actual, error) || actual != *expected) {
            if (!current.Valid()) error = ::GetLastError();
            current.Reset();
            ::DeleteFileW(temporary.c_str());
            return {.status = TextFileStatus::changed, .native_error = error};
        }
    }
    if (!::MoveFileExW(temporary.c_str(), path.c_str(),
                       MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        error = ::GetLastError();
        ::DeleteFileW(temporary.c_str());
        return {.status = StatusFromError(error), .native_error = error};
    }
    FileHandle saved{::CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr)};
    FileStamp stamp{};
    if (!saved.Valid() || !QueryStamp(saved.Get(), stamp, error)) {
        if (!saved.Valid()) error = ::GetLastError();
        return {.status = StatusFromError(error), .native_error = error};
    }
    return {.stamp = stamp, .status = TextFileStatus::success};
}

}  // namespace mwtl
