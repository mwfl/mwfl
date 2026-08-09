#include <mwtl/tab_workspace.h>

#include <cstdlib>

int main() {
    mwtl::TabWorkspaceModel model;
    const mwtl::TabId first{11};
    const mwtl::TabId second{22};
    const mwtl::TabId third{33};

    if (model.Add({{}, L"invalid", false, true}) || model.Add({first, L"", false, true}) ||
        !model.Add({first, L"First", false, false}) || model.GetSelectedId() != first ||
        model.Add({first, L"duplicate", false, true}) ||
        !model.Add({second, L"Second", false, true}) || !model.Add({third, L"Third", true, true}) ||
        model.GetCount() != 3) {
        return EXIT_FAILURE;
    }

    const auto initial_view = model.GetTabs();
    if (initial_view[0].closable || !initial_view[2].dirty || !model.Select(second) ||
        model.GetSelectedId() != second || model.Select({99}) || !model.SetDirty(second, true) ||
        !model.SetTitle(second, L"Renamed")) {
        return EXIT_FAILURE;
    }
    const auto* renamed = model.Find(second);
    if (renamed == nullptr || renamed->title != L"Renamed" || !renamed->dirty) {
        return EXIT_FAILURE;
    }

    if (!model.Move(third, 0) || model.GetTabs()[0].id != third ||
        model.GetSelectedId() != second || model.Move(first, 99) || !model.Remove(second) ||
        model.GetSelectedId() != first || model.Remove(second) || !model.Remove(first) ||
        model.GetSelectedId() != third || !model.Remove(third) || model.GetSelectedId() ||
        !model.Empty()) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
