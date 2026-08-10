#pragma once

#include <mwfl/navigation_controls.h>

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace explorer_example {

enum class FileColumn { name, type, size };

struct FolderEntry {
    mwfl::TreeItemId id{};
    mwfl::TreeItemId parent{};
    std::wstring name;
};

struct FileEntry {
    mwfl::ListItemId id{};
    mwfl::TreeItemId folder{};
    std::wstring name;
    std::wstring type;
    std::uint64_t size_bytes = 0;
    int image_index = -1;
};

class ExplorerModel final : public mwfl::VirtualListModel {
public:
    ExplorerModel();

    std::span<const FolderEntry> GetFolders() const noexcept { return folders_; }
    mwfl::TreeItemId GetSelectedFolder() const noexcept { return selected_folder_; }
    bool SelectFolder(mwfl::TreeItemId id);
    void Sort(FileColumn column, bool ascending);

    std::size_t GetRowCount() const noexcept override { return visible_.size(); }
    mwfl::ListItemId GetRowId(std::size_t row) const noexcept override;
    std::wstring GetCellText(std::size_t row, int column) const override;
    int GetImageIndex(std::size_t row, int column) const noexcept override;
    const FileEntry* GetFile(std::size_t row) const noexcept;

private:
    void RebuildVisible();

    std::vector<FolderEntry> folders_;
    std::vector<FileEntry> files_;
    std::vector<std::size_t> visible_;
    mwfl::TreeItemId selected_folder_{1};
    FileColumn sort_column_ = FileColumn::name;
    bool ascending_ = true;
};

}  // namespace explorer_example
