#include "../examples/compare_tool/compare_model.h"

#include <wil/resource.h>

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>

namespace {

void Write(const std::filesystem::path& path, std::string_view text) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary);
    stream << text;
}

const compare_tool::Entry* Find(const compare_tool::FolderResult& result,
                                std::wstring_view path) {
    const auto found = std::ranges::find(result.entries, path, [](const auto& entry) {
        return entry.relative_path.generic_wstring();
    });
    return found == result.entries.end() ? nullptr : &*found;
}

}  // namespace

int main() {
    const auto root = std::filesystem::temp_directory_path() /
                      (L"mwfl-compare-test-" + std::to_wstring(::GetCurrentProcessId()));
    const auto cleanup = wil::scope_exit([&] {
        std::error_code ignored;
        std::filesystem::remove_all(root, ignored);
    });
    const auto left = root / L"left";
    const auto right = root / L"right";
    Write(left / L"same.txt", "same\n");
    Write(right / L"same.txt", "same\n");
    Write(left / L"changed.txt", "first\nkeep\nleft\n");
    Write(right / L"changed.txt", "first\nkeep\nright\n");
    Write(left / L"left-only.txt", "copy me\n");
    Write(right / L"right-only.txt", "right\n");
    Write(left / L"build/ignored.obj", "a");
    Write(right / L"build/ignored.obj", "b");

    const auto folders = compare_tool::CompareFolders(
        left, right, {.include_same = true, .verify_contents = true});
    if (folders.same < 1 || folders.different != 1 || folders.left_only != 1 ||
        folders.right_only != 1 || Find(folders, L"build/ignored.obj") != nullptr)
        return 1;
    const auto* changed = Find(folders, L"changed.txt");
    const auto* only_left = Find(folders, L"left-only.txt");
    if (!changed || changed->status != compare_tool::Status::different || !only_left)
        return 2;

    const auto diff = compare_tool::CompareTextFiles(left / L"changed.txt",
                                                     right / L"changed.txt");
    if (!diff.error.empty() || diff.binary || diff.changed != 1 ||
        diff.lines.back().kind != compare_tool::LineKind::modified)
        return 3;
    Write(right / L"no-newline.txt", "value");
    Write(left / L"no-newline.txt", "value\n");
    const auto newline = compare_tool::CompareTextFiles(left / L"no-newline.txt",
                                                        right / L"no-newline.txt");
    if (newline.changed != 1 || newline.left_final_newline == newline.right_final_newline)
        return 4;

    std::wstring error;
    if (!compare_tool::CopyEntryVerified(left, right, *only_left, error) ||
        !std::filesystem::exists(right / L"left-only.txt"))
        return 5;
    const auto verified = compare_tool::CompareFolders(
        left, right, {.include_same = true, .verify_contents = true});
    const auto* copied = Find(verified, L"left-only.txt");
    if (!copied || copied->status != compare_tool::Status::same) return 6;

    std::atomic_bool cancelled{true};
    if (!compare_tool::CompareFolders(left, right, {}, &cancelled).cancelled) return 7;

    std::mt19937 random{12345};
    for (int iteration = 0; iteration < 100; ++iteration) {
        std::vector<std::string> first;
        std::vector<std::string> second;
        const int count = 3 + static_cast<int>(random() % 20);
        for (int index = 0; index < count; ++index)
            first.push_back("line-" + std::to_string(index));
        second = first;
        for (int edit = 0; edit < 5; ++edit) {
            const int operation = static_cast<int>(random() % 3);
            const std::size_t position = second.empty() ? 0 : random() % second.size();
            if (operation == 0 && !second.empty()) second.erase(second.begin() + position);
            else if (operation == 1)
                second.insert(second.begin() + position,
                              "insert-" + std::to_string(iteration) + "-" +
                                  std::to_string(edit));
            else if (!second.empty())
                second[position] = "changed-" + std::to_string(iteration) + "-" +
                                   std::to_string(edit);
        }
        const auto join = [](const std::vector<std::string>& lines) {
            std::string text;
            for (const auto& line : lines) text += line + "\n";
            return text;
        };
        Write(left / L"random.txt", join(first));
        Write(right / L"random.txt", join(second));
        const auto randomized = compare_tool::CompareTextFiles(left / L"random.txt",
                                                               right / L"random.txt");
        std::vector<std::wstring> rebuilt_left, rebuilt_right;
        for (const auto& line : randomized.lines) {
            if (line.left_number != 0) rebuilt_left.push_back(line.left);
            if (line.right_number != 0) rebuilt_right.push_back(line.right);
        }
        const auto widen = [](const std::vector<std::string>& lines) {
            std::vector<std::wstring> output;
            for (const auto& line : lines) output.emplace_back(line.begin(), line.end());
            return output;
        };
        if (!randomized.error.empty() || rebuilt_left != widen(first) ||
            rebuilt_right != widen(second))
            return 8;
    }
    return EXIT_SUCCESS;
}
