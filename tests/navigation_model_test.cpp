#include <mwfl/navigation_controls.h>

#include <string>
#include <vector>

namespace {

class Model final : public mwfl::VirtualListModel {
public:
    std::size_t GetRowCount() const noexcept override { return ids.size(); }
    mwfl::ListItemId GetRowId(std::size_t row) const noexcept override {
        return row < ids.size() ? ids[row] : mwfl::ListItemId{};
    }
    std::wstring GetCellText(std::size_t row, int column) const override {
        return std::to_wstring(GetRowId(row).value) + L":" + std::to_wstring(column);
    }

    std::vector<mwfl::ListItemId> ids{{40}, {10}, {70}};
};

}  // namespace

int main() {
    Model model;
    if (model.FindRow({40}) != 0 || model.FindRow({10}) != 1 ||
        model.FindRow({70}) != 2 || model.FindRow({99}) || model.FindRow({}))
        return 1;
    if (model.GetCellText(1, 3) != L"10:3") return 2;
    model.ids = {{70}, {40}, {10}};
    if (model.FindRow({10}) != 2 || model.FindRow({70}) != 0) return 3;
    return 0;
}
