#include "explorer_model.h"

#include <algorithm>
#include <format>
#include <utility>

namespace explorer_example {
namespace {

std::wstring FormatSize(std::uint64_t bytes) {
    if (bytes >= 1024 * 1024)
        return std::format(L"{} MB", (bytes + 512 * 1024) / (1024 * 1024));
    if (bytes >= 1024) return std::format(L"{} KB", (bytes + 512) / 1024);
    return std::format(L"{} bytes", bytes);
}

}  // namespace

ExplorerModel::ExplorerModel()
    : folders_{{{1}, {}, L"This PC"},
               {{10}, {1}, L"Documents"},
               {{20}, {1}, L"Pictures"},
               {{30}, {1}, L"Downloads"}},
      files_{{{1001}, {10}, L"Design notes.txt", L"Text document", 18 * 1024, 1},
             {{1002}, {10}, L"Roadmap.md", L"Markdown document", 42 * 1024, 1},
             {{2001}, {20}, L"Architecture.png", L"PNG image", 3 * 1024 * 1024, 2},
             {{2002}, {20}, L"Prototype.jpg", L"JPEG image", 5 * 1024 * 1024, 2},
             {{3001}, {30}, L"mwfl-samples.zip", L"ZIP archive", 8 * 1024 * 1024, 3},
             {{3002}, {30}, L"Release checklist.pdf", L"PDF document", 760 * 1024, 1}} {
    RebuildVisible();
}

bool ExplorerModel::SelectFolder(mwfl::TreeItemId id) {
    if (std::ranges::find(folders_, id, &FolderEntry::id) == folders_.end()) return false;
    selected_folder_ = id;
    RebuildVisible();
    return true;
}

void ExplorerModel::Sort(FileColumn column, bool ascending) {
    sort_column_ = column;
    ascending_ = ascending;
    RebuildVisible();
}

mwfl::ListItemId ExplorerModel::GetRowId(std::size_t row) const noexcept {
    const FileEntry* file = GetFile(row);
    return file == nullptr ? mwfl::ListItemId{} : file->id;
}

std::wstring ExplorerModel::GetCellText(std::size_t row, int column) const {
    const FileEntry* file = GetFile(row);
    if (file == nullptr) return {};
    switch (column) {
        case 0: return file->name;
        case 1: return file->type;
        case 2: return FormatSize(file->size_bytes);
        default: return {};
    }
}

int ExplorerModel::GetImageIndex(std::size_t row, int column) const noexcept {
    const FileEntry* file = GetFile(row);
    return file != nullptr && column == 0 ? file->image_index : -1;
}

const FileEntry* ExplorerModel::GetFile(std::size_t row) const noexcept {
    if (row >= visible_.size() || visible_[row] >= files_.size()) return nullptr;
    return &files_[visible_[row]];
}

void ExplorerModel::RebuildVisible() {
    visible_.clear();
    for (std::size_t index = 0; index < files_.size(); ++index) {
        if (selected_folder_.value == 1 || files_[index].folder == selected_folder_)
            visible_.push_back(index);
    }
    const auto compare = [this](std::size_t left_index, std::size_t right_index) {
        const FileEntry& left = files_[left_index];
        const FileEntry& right = files_[right_index];
        int ordering = 0;
        switch (sort_column_) {
            case FileColumn::name: ordering = left.name.compare(right.name); break;
            case FileColumn::type: ordering = left.type.compare(right.type); break;
            case FileColumn::size:
                ordering = left.size_bytes < right.size_bytes ? -1
                           : left.size_bytes > right.size_bytes ? 1 : 0;
                break;
        }
        if (ordering == 0) ordering = left.id.value < right.id.value ? -1 :
                                      left.id.value > right.id.value ? 1 : 0;
        return ascending_ ? ordering < 0 : ordering > 0;
    };
    std::ranges::sort(visible_, compare);
}

}  // namespace explorer_example
