#include <mwtl/text_file.h>

#include <windows.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {
class TemporaryDirectory final {
public:
    TemporaryDirectory() {
        wchar_t base[MAX_PATH]{};
        if (::GetTempPathW(MAX_PATH, base) == 0) return;
        path_ = std::filesystem::path{base} /
            (L"mwtl-text-file-" + std::to_wstring(::GetCurrentProcessId()) + L"-" +
             std::to_wstring(::GetTickCount64()));
        std::error_code error;
        std::filesystem::create_directory(path_, error);
        if (error) path_.clear();
    }
    ~TemporaryDirectory() {
        if (path_.empty()) return;
        std::error_code ignored;
        std::filesystem::remove_all(path_, ignored);
    }
    const std::filesystem::path& Get() const noexcept { return path_; }
private:
    std::filesystem::path path_;
};

bool RoundTrip(const std::filesystem::path& path, mwtl::TextEncoding encoding,
               std::wstring_view text) {
    const auto written = mwtl::WriteTextFileAtomic(path, text, encoding);
    if (!written.Succeeded() || !written.stamp) return false;
    const auto read = mwtl::ReadTextFile(path);
    return read.Succeeded() && read.value && read.value->text == text &&
           read.value->encoding == encoding && read.value->stamp == *written.stamp;
}

bool WriteBytes(const std::filesystem::path& path, const std::vector<unsigned char>& bytes) {
    std::ofstream output{path, std::ios::binary | std::ios::trunc};
    output.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    return output.good();
}
}  // namespace

int main() {
    TemporaryDirectory temporary;
    if (temporary.Get().empty()) return EXIT_FAILURE;
    const std::wstring sample = L"Hello, 世界 — mwtl\r\nSecond line 😀";
    if (!RoundTrip(temporary.Get() / L"utf8.txt", mwtl::TextEncoding::utf8, sample) ||
        !RoundTrip(temporary.Get() / L"utf8-bom.txt", mwtl::TextEncoding::utf8_bom, sample) ||
        !RoundTrip(temporary.Get() / L"utf16-le.txt", mwtl::TextEncoding::utf16_le, sample) ||
        !RoundTrip(temporary.Get() / L"utf16-be.txt", mwtl::TextEncoding::utf16_be, sample) ||
        !RoundTrip(temporary.Get() / L"empty.txt", mwtl::TextEncoding::utf8, L"")) return EXIT_FAILURE;

    std::wstring large(2 * 1024 * 1024, L'x');
    large[1024] = L'界';
    if (!RoundTrip(temporary.Get() / L"large.txt", mwtl::TextEncoding::utf8, large)) return EXIT_FAILURE;

    const auto missing = mwtl::ReadTextFile(temporary.Get() / L"missing.txt");
    if (missing.status != mwtl::TextFileStatus::not_found || missing.native_error == 0) return EXIT_FAILURE;

    const auto malformed_path = temporary.Get() / L"malformed.txt";
    if (!WriteBytes(malformed_path, {0xF0, 0x28, 0x8C, 0x28}) ||
        mwtl::ReadTextFile(malformed_path).status != mwtl::TextFileStatus::invalid_encoding) return EXIT_FAILURE;
    const auto malformed_utf16 = temporary.Get() / L"malformed-utf16.txt";
    if (!WriteBytes(malformed_utf16, {0xFF, 0xFE, 0x00, 0xD8}) ||
        mwtl::ReadTextFile(malformed_utf16).status != mwtl::TextFileStatus::invalid_encoding) return EXIT_FAILURE;

    const auto conflict_path = temporary.Get() / L"conflict.txt";
    if (!mwtl::WriteTextFileAtomic(conflict_path, L"first").Succeeded()) return EXIT_FAILURE;
    const auto original = mwtl::ReadTextFile(conflict_path);
    if (!original.Succeeded() || !original.value) return EXIT_FAILURE;
    if (!mwtl::WriteTextFileAtomic(conflict_path, L"external and longer").Succeeded()) return EXIT_FAILURE;
    const auto conflict = mwtl::WriteTextFileAtomic(
        conflict_path, L"stale editor", mwtl::TextEncoding::utf8, original.value->stamp);
    if (conflict.status != mwtl::TextFileStatus::changed) return EXIT_FAILURE;
    const auto preserved = mwtl::ReadTextFile(conflict_path);
    if (!preserved.Succeeded() || preserved.value->text != L"external and longer") return EXIT_FAILURE;

    const auto read_only_path = temporary.Get() / L"read-only.txt";
    if (!mwtl::WriteTextFileAtomic(read_only_path, L"preserve me").Succeeded() ||
        !::SetFileAttributesW(read_only_path.c_str(), FILE_ATTRIBUTE_READONLY)) return EXIT_FAILURE;
    const auto read_only_write = mwtl::WriteTextFileAtomic(read_only_path, L"must fail");
    ::SetFileAttributesW(read_only_path.c_str(), FILE_ATTRIBUTE_NORMAL);
    if (read_only_write.Succeeded()) return EXIT_FAILURE;
    const auto read_only_preserved = mwtl::ReadTextFile(read_only_path);
    if (!read_only_preserved.Succeeded() || read_only_preserved.value->text != L"preserve me") return EXIT_FAILURE;

    if (mwtl::WriteTextFileAtomic(temporary.Get(), L"cannot replace a directory").Succeeded()) return EXIT_FAILURE;
    for (const auto& entry : std::filesystem::directory_iterator{temporary.Get()}) {
        if (entry.path().filename().wstring().find(L".mwtl-tmp-") != std::wstring::npos) return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
