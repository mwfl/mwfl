#include "../examples/explorer/explorer_model.h"

#include <cstdlib>

int main() {
    using namespace explorer_example;
    using mwfl::ListItemId;
    using mwfl::TreeItemId;

    ExplorerModel model;
    if (model.GetFolders().size() != 4 || model.GetSelectedFolder() != TreeItemId{1} ||
        model.GetRowCount() != 6 || model.GetRowId(0) != ListItemId{1001} ||
        model.FindRow({3001}) != 4)
        return 1;
    if (!model.SelectFolder({20}) || model.GetRowCount() != 2 ||
        model.GetRowId(0) != ListItemId{2001} || model.GetCellText(0, 1) != L"PNG image" ||
        model.GetCellText(0, 2) != L"3 MB" || model.GetImageIndex(0, 0) != 2 ||
        model.GetImageIndex(0, 1) != -1)
        return 2;

    model.Sort(FileColumn::size, false);
    if (model.GetRowId(0) != ListItemId{2001} || model.GetRowId(1) != ListItemId{2002} ||
        model.FindRow({2001}) != 0)
        return 3;
    model.Sort(FileColumn::name, true);
    if (model.GetRowId(0) != ListItemId{2001} || model.GetRowId(1) != ListItemId{2002})
        return 4;

    if (model.SelectFolder({999}) || model.GetSelectedFolder() != TreeItemId{20} ||
        model.GetRowCount() != 2 || model.GetRowId(99) || !model.GetCellText(99, 0).empty() ||
        model.GetFile(99) != nullptr)
        return 5;
    if (!model.SelectFolder({30}) || model.GetRowCount() != 2 ||
        model.FindRow({3001}) == std::nullopt || model.FindRow({2001}) != std::nullopt)
        return 6;

    return EXIT_SUCCESS;
}
