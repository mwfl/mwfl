#include <mwtl/selection.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct FakeSelectionControl {
    int AddItem(std::wstring_view label) {
        labels.emplace_back(label);
        return static_cast<int>(labels.size()) - 1;
    }
    int GetItemCount() const noexcept { return static_cast<int>(labels.size()); }
    std::optional<int> GetSelectedIndex() const noexcept { return selected; }
    bool SetSelection(int index) noexcept {
        if (index < 0 || index >= GetItemCount()) return false;
        selected = index;
        return true;
    }
    bool RemoveItem(int index) noexcept {
        if (index < 0 || index >= GetItemCount()) return false;
        labels.erase(labels.begin() + index);
        if (selected && *selected == index) selected.reset();
        else if (selected && *selected > index) --*selected;
        return true;
    }
    bool ClearItems() noexcept {
        labels.clear();
        selected.reset();
        return true;
    }

    std::vector<std::wstring> labels;
    std::optional<int> selected;
};

}  // namespace

int main() {
    FakeSelectionControl control;
    mwtl::SelectionAdapter<FakeSelectionControl, int> adapter{control};
    if (!adapter.Add(L"ten", 10) || !adapter.Add(L"twenty", 20) ||
        adapter.Size() != 2 || !adapter.IsSynchronized()) return 1;
    if (!adapter.SelectValue(20) || !adapter.GetSelectedValue() ||
        adapter.GetSelectedValue()->get() != 20) return 2;
    if (adapter.Select(2) || ::GetLastError() != ERROR_INVALID_INDEX ||
        adapter.SelectValue(99) || ::GetLastError() != ERROR_NOT_FOUND) return 3;
    if (!adapter.Remove(0) || adapter.Size() != 1 || adapter.Get(0) == nullptr ||
        adapter.Get(0)->label != L"twenty" || adapter.Get(0)->value != 20) return 4;

    control.labels.emplace_back(L"external mutation");
    if (adapter.IsSynchronized() || adapter.Add(L"rejected", 30) ||
        ::GetLastError() != ERROR_INVALID_STATE || adapter.Size() != 1) return 5;
    control.labels.pop_back();
    if (!adapter.Clear() || adapter.Size() != 0 || control.GetItemCount() != 0) return 6;
    return 0;
}
