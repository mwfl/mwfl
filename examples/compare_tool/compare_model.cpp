#include "compare_model.h"

#include <windows.h>
#include <shlwapi.h>

#include <algorithm>
#include <array>
#include <cwctype>
#include <fstream>
#include <map>
#include <set>
#include <system_error>

namespace compare_tool {
namespace {

struct CaseInsensitiveLess {
    bool operator()(const std::wstring& left, const std::wstring& right) const noexcept {
        return _wcsicmp(left.c_str(), right.c_str()) < 0;
    }
};

struct FileInfo {
    std::filesystem::file_type type = std::filesystem::file_type::none;
    std::uintmax_t size = 0;
    std::filesystem::path path;
    std::wstring error;
};

std::wstring ErrorCodeText(const std::error_code& error) {
    return L"Filesystem error " + std::to_wstring(error.value());
}

std::vector<std::wstring> SplitPatterns(std::wstring_view patterns) {
    std::vector<std::wstring> output;
    std::size_t start = 0;
    while (start <= patterns.size()) {
        const std::size_t end = patterns.find(L';', start);
        std::wstring value{patterns.substr(start, end == std::wstring_view::npos
                                                     ? patterns.size() - start
                                                     : end - start)};
        value.erase(value.begin(), std::find_if(value.begin(), value.end(), [](wchar_t c) {
                        return !std::iswspace(c);
                    }));
        while (!value.empty() && std::iswspace(value.back())) value.pop_back();
        if (!value.empty()) output.push_back(std::move(value));
        if (end == std::wstring_view::npos) break;
        start = end + 1;
    }
    return output;
}

bool Excluded(const std::filesystem::path& relative,
              const std::vector<std::wstring>& patterns) {
    const std::wstring filename = relative.filename().wstring();
    const std::wstring generic = relative.generic_wstring();
    return std::ranges::any_of(patterns, [&](const std::wstring& pattern) {
        return ::PathMatchSpecW(filename.c_str(), pattern.c_str()) != FALSE ||
               ::PathMatchSpecW(generic.c_str(), pattern.c_str()) != FALSE;
    });
}

using FileMap = std::map<std::wstring, FileInfo, CaseInsensitiveLess>;

FileMap Scan(const std::filesystem::path& root,
             const std::vector<std::wstring>& exclusions,
             const std::atomic_bool* cancel, Progress& progress,
             std::size_t& scanned) {
    FileMap files;
    std::error_code error;
    std::filesystem::recursive_directory_iterator iterator(
        root, std::filesystem::directory_options::skip_permission_denied, error);
    const std::filesystem::recursive_directory_iterator end;
    if (error) {
        files.emplace(L"", FileInfo{.error = ErrorCodeText(error)});
        return files;
    }
    for (; iterator != end; iterator.increment(error)) {
        if (cancel != nullptr && cancel->load(std::memory_order_relaxed)) break;
        if (error) {
            error.clear();
            continue;
        }
        const auto relative = iterator->path().lexically_relative(root);
        if (Excluded(relative, exclusions)) {
            if (iterator->is_directory(error)) iterator.disable_recursion_pending();
            error.clear();
            continue;
        }
        FileInfo info;
        info.path = iterator->path();
        info.type = iterator->symlink_status(error).type();
        if (error) {
            info.error = ErrorCodeText(error);
            error.clear();
        } else if (info.type == std::filesystem::file_type::regular) {
            info.size = iterator->file_size(error);
            if (error) {
                info.error = ErrorCodeText(error);
                error.clear();
            }
        }
        files.emplace(relative.generic_wstring(), std::move(info));
        ++scanned;
        if (progress && scanned % 128 == 0) progress(scanned, relative.generic_wstring());
    }
    return files;
}

bool EqualFiles(const std::filesystem::path& left, const std::filesystem::path& right,
                const std::atomic_bool* cancel, std::wstring& error) {
    std::ifstream first(left, std::ios::binary);
    std::ifstream second(right, std::ios::binary);
    if (!first || !second) {
        error = L"Could not open one of the files.";
        return false;
    }
    std::array<char, 64 * 1024> a{}, b{};
    while (first && second) {
        if (cancel != nullptr && cancel->load(std::memory_order_relaxed)) return false;
        first.read(a.data(), static_cast<std::streamsize>(a.size()));
        second.read(b.data(), static_cast<std::streamsize>(b.size()));
        const auto first_count = first.gcount();
        const auto second_count = second.gcount();
        if (first_count != second_count ||
            !std::equal(a.begin(), a.begin() + first_count, b.begin()))
            return false;
    }
    if (first.bad() || second.bad()) error = L"Reading a file failed.";
    return error.empty();
}

struct LoadedText {
    std::vector<std::wstring> lines;
    bool final_newline = false;
    bool binary = false;
    std::wstring error;
};

std::wstring DecodeText(const std::vector<char>& bytes, std::wstring& error) {
    if (bytes.empty()) return {};
    const auto decode = [&](UINT code_page, DWORD flags, const char* data, int length) {
        const int count = ::MultiByteToWideChar(code_page, flags, data, length, nullptr, 0);
        if (count <= 0) return std::wstring{};
        std::wstring output(static_cast<std::size_t>(count), L'\0');
        ::MultiByteToWideChar(code_page, flags, data, length, output.data(), count);
        return output;
    };
    if (bytes.size() >= 2 && static_cast<unsigned char>(bytes[0]) == 0xFF &&
        static_cast<unsigned char>(bytes[1]) == 0xFE) {
        const auto* begin = reinterpret_cast<const wchar_t*>(bytes.data() + 2);
        return {begin, begin + (bytes.size() - 2) / sizeof(wchar_t)};
    }
    std::size_t start = bytes.size() >= 3 &&
                                static_cast<unsigned char>(bytes[0]) == 0xEF &&
                                static_cast<unsigned char>(bytes[1]) == 0xBB &&
                                static_cast<unsigned char>(bytes[2]) == 0xBF
                            ? 3
                            : 0;
    auto output = decode(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data() + start,
                         static_cast<int>(bytes.size() - start));
    if (!output.empty() || bytes.size() == start) return output;
    output = decode(CP_ACP, 0, bytes.data(), static_cast<int>(bytes.size()));
    if (output.empty()) error = L"The text encoding could not be decoded.";
    return output;
}

LoadedText LoadText(const std::filesystem::path& path, std::size_t maximum_bytes) {
    LoadedText loaded;
    std::error_code ec;
    const auto size = std::filesystem::file_size(path, ec);
    if (ec || size > maximum_bytes) {
        loaded.error = ec ? L"Could not read the file size." : L"File exceeds the text preview limit.";
        return loaded;
    }
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        loaded.error = L"Could not open the text file.";
        return loaded;
    }
    std::vector<char> bytes(static_cast<std::size_t>(size));
    stream.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!stream && !bytes.empty()) {
        loaded.error = L"Could not read the text file.";
        return loaded;
    }
    loaded.binary = std::find(bytes.begin(), bytes.end(), '\0') != bytes.end();
    if (loaded.binary) return loaded;
    std::wstring text = DecodeText(bytes, loaded.error);
    if (!loaded.error.empty()) return loaded;
    loaded.final_newline = !text.empty() && (text.back() == L'\n' || text.back() == L'\r');
    std::size_t start = 0;
    while (start < text.size()) {
        const auto end = text.find_first_of(L"\r\n", start);
        loaded.lines.push_back(text.substr(start, end == std::wstring::npos
                                                     ? text.size() - start
                                                     : end - start));
        if (end == std::wstring::npos) break;
        start = end + 1;
        if (text[end] == L'\r' && start < text.size() && text[start] == L'\n') ++start;
    }
    if (loaded.final_newline && (loaded.lines.empty() || start < text.size()))
        loaded.lines.emplace_back();
    return loaded;
}

std::vector<DiffLine> Myers(const std::vector<std::wstring>& left,
                            const std::vector<std::wstring>& right) {
    const int n = static_cast<int>(left.size());
    const int m = static_cast<int>(right.size());
    const int maximum = n + m;
    const int offset = maximum + 1;
    std::vector<int> v(static_cast<std::size_t>(2 * maximum + 3), 0);
    v[static_cast<std::size_t>(offset + 1)] = 0;
    std::vector<std::vector<int>> trace;
    int distance = 0;
    bool finished = false;
    for (int d = 0; d <= maximum && !finished; ++d) {
        trace.push_back(v);
        for (int k = -d; k <= d; k += 2) {
            const int index = offset + k;
            int x = k == -d || (k != d && v[static_cast<std::size_t>(index - 1)] <
                                               v[static_cast<std::size_t>(index + 1)])
                        ? v[static_cast<std::size_t>(index + 1)]
                        : v[static_cast<std::size_t>(index - 1)] + 1;
            int y = x - k;
            while (x < n && y < m && left[static_cast<std::size_t>(x)] ==
                                           right[static_cast<std::size_t>(y)]) {
                ++x;
                ++y;
            }
            v[static_cast<std::size_t>(index)] = x;
            if (x >= n && y >= m) {
                distance = d;
                finished = true;
                break;
            }
        }
    }

    std::vector<DiffLine> reversed;
    int x = n, y = m;
    for (int d = distance; d > 0; --d) {
        const auto& previous = trace[static_cast<std::size_t>(d)];
        const int k = x - y;
        const int index = offset + k;
        const int previous_k =
            k == -d || (k != d && previous[static_cast<std::size_t>(index - 1)] <
                                      previous[static_cast<std::size_t>(index + 1)])
                ? k + 1
                : k - 1;
        const int previous_x = previous[static_cast<std::size_t>(offset + previous_k)];
        const int previous_y = previous_x - previous_k;
        while (x > previous_x && y > previous_y) {
            --x;
            --y;
            reversed.push_back({static_cast<std::size_t>(x + 1),
                                static_cast<std::size_t>(y + 1),
                                left[static_cast<std::size_t>(x)],
                                right[static_cast<std::size_t>(y)], LineKind::same});
        }
        if (x == previous_x) {
            --y;
            reversed.push_back({0, static_cast<std::size_t>(y + 1), L"",
                                right[static_cast<std::size_t>(y)], LineKind::right_only});
        } else {
            --x;
            reversed.push_back({static_cast<std::size_t>(x + 1), 0,
                                left[static_cast<std::size_t>(x)], L"",
                                LineKind::left_only});
        }
    }
    while (x > 0 && y > 0) {
        --x;
        --y;
        reversed.push_back({static_cast<std::size_t>(x + 1),
                            static_cast<std::size_t>(y + 1),
                            left[static_cast<std::size_t>(x)],
                            right[static_cast<std::size_t>(y)], LineKind::same});
    }
    while (x > 0) {
        --x;
        reversed.push_back({static_cast<std::size_t>(x + 1), 0,
                            left[static_cast<std::size_t>(x)], L"", LineKind::left_only});
    }
    while (y > 0) {
        --y;
        reversed.push_back({0, static_cast<std::size_t>(y + 1), L"",
                            right[static_cast<std::size_t>(y)], LineKind::right_only});
    }
    std::ranges::reverse(reversed);

    std::vector<DiffLine> combined;
    for (std::size_t index = 0; index < reversed.size(); ++index) {
        if (index + 1 < reversed.size() && reversed[index].kind == LineKind::left_only &&
            reversed[index + 1].kind == LineKind::right_only) {
            combined.push_back({reversed[index].left_number, reversed[index + 1].right_number,
                                std::move(reversed[index].left),
                                std::move(reversed[index + 1].right), LineKind::modified});
            ++index;
        } else {
            combined.push_back(std::move(reversed[index]));
        }
    }
    return combined;
}

std::vector<DiffLine> AlignedFallback(const std::vector<std::wstring>& left,
                                      const std::vector<std::wstring>& right) {
    std::vector<DiffLine> lines;
    const std::size_t count = (std::max)(left.size(), right.size());
    lines.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        const bool has_left = index < left.size();
        const bool has_right = index < right.size();
        const auto kind = !has_left        ? LineKind::right_only
                          : !has_right     ? LineKind::left_only
                          : left[index] == right[index] ? LineKind::same
                                                       : LineKind::modified;
        lines.push_back({has_left ? index + 1 : 0, has_right ? index + 1 : 0,
                         has_left ? left[index] : L"", has_right ? right[index] : L"",
                         kind});
    }
    return lines;
}

}  // namespace

FolderResult CompareFolders(const std::filesystem::path& left,
                            const std::filesystem::path& right,
                            const FolderOptions& options,
                            const std::atomic_bool* cancel, Progress progress) {
    FolderResult result;
    const auto exclusions = SplitPatterns(options.exclusions);
    std::size_t scanned = 0;
    const FileMap left_files = Scan(left, exclusions, cancel, progress, scanned);
    const FileMap right_files = Scan(right, exclusions, cancel, progress, scanned);
    if (cancel != nullptr && cancel->load(std::memory_order_relaxed)) {
        result.cancelled = true;
        return result;
    }
    std::set<std::wstring, CaseInsensitiveLess> paths;
    for (const auto& [path, info] : left_files) paths.insert(path);
    for (const auto& [path, info] : right_files) paths.insert(path);
    for (const auto& path : paths) {
        if (cancel != nullptr && cancel->load(std::memory_order_relaxed)) {
            result.cancelled = true;
            break;
        }
        const auto left_found = left_files.find(path);
        const auto right_found = right_files.find(path);
        Entry entry{.relative_path = path};
        if (left_found == left_files.end()) {
            entry.status = Status::right_only;
            entry.right_directory = right_found->second.type == std::filesystem::file_type::directory;
            entry.right_size = right_found->second.size;
            ++result.right_only;
        } else if (right_found == right_files.end()) {
            entry.status = Status::left_only;
            entry.left_directory = left_found->second.type == std::filesystem::file_type::directory;
            entry.left_size = left_found->second.size;
            ++result.left_only;
        } else {
            const FileInfo& a = left_found->second;
            const FileInfo& b = right_found->second;
            entry.left_directory = a.type == std::filesystem::file_type::directory;
            entry.right_directory = b.type == std::filesystem::file_type::directory;
            entry.left_size = a.size;
            entry.right_size = b.size;
            if (!a.error.empty() || !b.error.empty()) {
                entry.status = Status::error;
                entry.detail = !a.error.empty() ? a.error : b.error;
                ++result.errors;
            } else if (a.type != b.type) {
                entry.status = Status::type_conflict;
                ++result.different;
            } else if (a.type == std::filesystem::file_type::regular) {
                bool same = a.size == b.size;
                std::wstring read_error;
                if (same && options.verify_contents)
                    same = EqualFiles(a.path, b.path, cancel, read_error);
                if (!read_error.empty()) {
                    entry.status = Status::error;
                    entry.detail = read_error;
                    ++result.errors;
                } else {
                    entry.status = same ? Status::same : Status::different;
                    same ? ++result.same : ++result.different;
                }
            } else if (a.type == std::filesystem::file_type::symlink) {
                std::error_code first_error, second_error;
                const auto first = std::filesystem::read_symlink(a.path, first_error);
                const auto second = std::filesystem::read_symlink(b.path, second_error);
                entry.status = !first_error && !second_error && first == second
                                   ? Status::same
                                   : Status::different;
                entry.status == Status::same ? ++result.same : ++result.different;
            } else {
                entry.status = Status::same;
                ++result.same;
            }
        }
        if (options.include_same || entry.status != Status::same)
            result.entries.push_back(std::move(entry));
    }
    return result;
}

TextDiff CompareTextFiles(const std::filesystem::path& left,
                          const std::filesystem::path& right,
                          std::size_t maximum_bytes) {
    TextDiff result;
    const LoadedText first = LoadText(left, maximum_bytes);
    const LoadedText second = LoadText(right, maximum_bytes);
    result.left_final_newline = first.final_newline;
    result.right_final_newline = second.final_newline;
    result.binary = first.binary || second.binary;
    if (!first.error.empty() || !second.error.empty()) {
        result.error = !first.error.empty() ? first.error : second.error;
        return result;
    }
    if (result.binary) {
        result.error = L"Binary files can be validated but not shown as text.";
        return result;
    }
    result.lines = first.lines.size() + second.lines.size() <= 20000
                       ? Myers(first.lines, second.lines)
                       : AlignedFallback(first.lines, second.lines);
    result.changed = static_cast<std::size_t>(std::ranges::count_if(
        result.lines, [](const DiffLine& line) { return line.kind != LineKind::same; }));
    if (first.final_newline != second.final_newline) ++result.changed;
    return result;
}

bool CopyEntryVerified(const std::filesystem::path& source_root,
                       const std::filesystem::path& destination_root,
                       const Entry& entry, std::wstring& error) noexcept {
    try {
        const auto normalized = entry.relative_path.lexically_normal();
        if (normalized.empty() || normalized.is_absolute() ||
            (!normalized.empty() && *normalized.begin() == L"..")) {
            error = L"The comparison entry is not a safe relative path.";
            return false;
        }
        const auto source = source_root / entry.relative_path;
        const auto destination = destination_root / entry.relative_path;
        std::error_code ec;
        const auto copy_file = [&](const std::filesystem::path& from,
                                   const std::filesystem::path& to) {
            std::filesystem::create_directories(to.parent_path(), ec);
            if (ec) {
                error = ErrorCodeText(ec);
                return false;
            }
            const auto temporary = to.wstring() + L".mwfl-copying";
            std::filesystem::copy_file(from, temporary,
                                       std::filesystem::copy_options::overwrite_existing, ec);
            if (ec) {
                error = ErrorCodeText(ec);
                return false;
            }
            std::wstring verify_error;
            if (!EqualFiles(from, temporary, nullptr, verify_error)) {
                std::filesystem::remove(temporary, ec);
                error = verify_error.empty() ? L"Copied bytes did not verify." : verify_error;
                return false;
            }
            if (!::MoveFileExW(temporary.c_str(), to.c_str(),
                               MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
                const DWORD code = ::GetLastError();
                std::filesystem::remove(temporary, ec);
                error = L"MoveFileEx failed with Win32 error " + std::to_wstring(code) + L".";
                return false;
            }
            return true;
        };
        if (std::filesystem::is_directory(source, ec)) {
            std::filesystem::create_directories(destination, ec);
            if (ec) {
                error = ErrorCodeText(ec);
                return false;
            }
            for (std::filesystem::recursive_directory_iterator iterator(
                     source, std::filesystem::directory_options::skip_permission_denied, ec),
                 end;
                 iterator != end; iterator.increment(ec)) {
                if (ec) {
                    error = ErrorCodeText(ec);
                    return false;
                }
                const auto relative = iterator->path().lexically_relative(source);
                const auto target = destination / relative;
                const auto status = iterator->symlink_status(ec);
                if (ec) {
                    error = ErrorCodeText(ec);
                    return false;
                }
                if (status.type() == std::filesystem::file_type::directory) {
                    std::filesystem::create_directories(target, ec);
                    if (ec) {
                        error = ErrorCodeText(ec);
                        return false;
                    }
                } else if (status.type() == std::filesystem::file_type::regular) {
                    if (!copy_file(iterator->path(), target)) return false;
                } else {
                    error = L"Directory copy does not follow symbolic links or special files.";
                    return false;
                }
            }
            error.clear();
            return true;
        }
        if (!copy_file(source, destination)) return false;
        error.clear();
        return true;
    } catch (const std::exception&) {
        error = L"Copy failed with a filesystem exception.";
        return false;
    } catch (...) {
        error = L"Copy failed.";
        return false;
    }
}

std::wstring StatusText(Status status) {
    switch (status) {
        case Status::same: return L"Same";
        case Status::different: return L"Different";
        case Status::left_only: return L"Left only";
        case Status::right_only: return L"Right only";
        case Status::type_conflict: return L"Type conflict";
        case Status::error: return L"Error";
    }
    return L"Unknown";
}

}  // namespace compare_tool
