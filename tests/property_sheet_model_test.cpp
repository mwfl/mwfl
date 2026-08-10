#include <mwtl/property_sheet.h>

#include <cstdlib>
#include <string>

int main() {
    using namespace mwtl;

    PropertySheetModel model;
    if (model.Add({{}, L"invalid"}) || !model.Add({{1}, L"General"}) ||
        !model.Add({{2}, L"Advanced"}) || model.Add({{1}, L"Duplicate"}) ||
        model.GetSelectedId() != PropertyPageId{1})
        return 1;

    if (!model.Select({2}) || model.GetSelectedId() != PropertyPageId{2} || model.Select({99}))
        return 2;
    if (!model.SetDirty({1}) || !model.SetDirty({2}) || !model.AnyDirty()) return 3;
    if (!model.Apply({1}, PropertyPageApplyOutcome::validation_failed) || !model.Find({1})->dirty)
        return 4;
    if (!model.Apply({1}, PropertyPageApplyOutcome::failed) || !model.Find({1})->dirty) return 5;
    if (!model.Apply({1}, PropertyPageApplyOutcome::applied) || model.Find({1})->dirty ||
        !model.AnyDirty())
        return 6;

    if (!model.Remove({2}) || model.GetSelectedId() != PropertyPageId{1} || model.Remove({99}))
        return 7;
    model.Reset();
    if (model.AnyDirty() || model.GetPages().size() != 1) return 8;
    if (!model.Remove({1}) || model.GetSelectedId().has_value() || !model.GetPages().empty())
        return 9;

    for (std::uint64_t id = 1; id <= 200; ++id) {
        if (!model.Add({{id}, L"Page " + std::to_wstring(id)})) return 10;
    }
    for (std::uint64_t id = 1; id <= 200; ++id) {
        if (!model.SetDirty({id}, id % 2 == 0)) return 11;
    }
    for (const auto& page : model.GetPages()) {
        if (page.dirty != (page.id.value % 2 == 0)) return 12;
    }
    if (!model.Select({100}) || !model.Remove({100}) ||
        model.GetSelectedId() != PropertyPageId{101})
        return 13;
    if (!model.Select({200}) || !model.Remove({200}) ||
        model.GetSelectedId() != PropertyPageId{199})
        return 14;
    model.Reset();
    if (model.AnyDirty() || model.GetPages().size() != 198) return 15;

    return EXIT_SUCCESS;
}
