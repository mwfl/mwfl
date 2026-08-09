#include <mwtl/splitter.h>

#include <cmath>
#include <cstdlib>

namespace {

bool Near(float actual, float expected) {
    return std::abs(actual - expected) < 0.001f;
}

}  // namespace

int main() {
    using namespace mwtl;

    SplitterModel splitter(SplitterOrientation::vertical, {Dip(100.0f), Dip(80.0f), Dip(6.0f)});
    splitter.SetPosition(Dip(120.0f));
    auto layout = splitter.Arrange({Dip(400.0f), Dip(240.0f)});
    if (!layout.constraints_satisfied || !Near(layout.first.size.width.value, 120.0f) ||
        !Near(layout.bar.origin.x.value, 120.0f) || !Near(layout.second.size.width.value, 274.0f))
        return 1;

    splitter.SetPosition(Dip(5.0f));
    layout = splitter.Arrange({Dip(400.0f), Dip(240.0f)});
    if (!Near(layout.first.size.width.value, 100.0f)) return 2;
    splitter.SetPosition(Dip(999.0f));
    layout = splitter.Arrange({Dip(400.0f), Dip(240.0f)});
    if (!Near(layout.first.size.width.value, 314.0f)) return 3;

    layout = splitter.Arrange({Dip(96.0f), Dip(20.0f)});
    if (layout.constraints_satisfied || !Near(layout.first.size.width.value, 50.0f) ||
        !Near(layout.second.size.width.value, 40.0f))
        return 4;

    splitter.SetOrientation(SplitterOrientation::horizontal);
    splitter.SetRatio(0.25f, {Dip(500.0f), Dip(406.0f)});
    layout = splitter.Arrange({Dip(500.0f), Dip(406.0f)});
    if (!Near(layout.first.size.height.value, 100.0f) || !Near(layout.bar.origin.y.value, 100.0f) ||
        !Near(layout.second.origin.y.value, 106.0f))
        return 5;

    splitter.Adjust(Dip(10.0f));
    layout = splitter.Arrange({Dip(500.0f), Dip(406.0f)});
    if (!Near(layout.first.size.height.value, 110.0f)) return 6;

    splitter.SetConstraints({Dip(-1.0f), Dip(-2.0f), Dip(-3.0f)});
    splitter.SetPosition(Dip(-10.0f));
    layout = splitter.Arrange({Dip(-5.0f), Dip(-5.0f)});
    if (!layout.constraints_satisfied || !Near(layout.bar.size.height.value, 0.0f)) return 7;

    for (const SplitterOrientation orientation :
         {SplitterOrientation::vertical, SplitterOrientation::horizontal}) {
        SplitterModel property_model(orientation, {Dip(20.0f), Dip(30.0f), Dip(5.0f)});
        for (int extent = 0; extent <= 200; ++extent) {
            for (int position = -20; position <= 220; position += 7) {
                property_model.SetPosition(Dip(static_cast<float>(position)));
                const SizeDip size = orientation == SplitterOrientation::vertical
                                         ? SizeDip{Dip(static_cast<float>(extent)), Dip(90.0f)}
                                         : SizeDip{Dip(90.0f), Dip(static_cast<float>(extent))};
                const SplitterLayout candidate = property_model.Arrange(size);
                const float first_size = orientation == SplitterOrientation::vertical
                                             ? candidate.first.size.width.value
                                             : candidate.first.size.height.value;
                const float bar_size = orientation == SplitterOrientation::vertical
                                           ? candidate.bar.size.width.value
                                           : candidate.bar.size.height.value;
                const float second_size = orientation == SplitterOrientation::vertical
                                              ? candidate.second.size.width.value
                                              : candidate.second.size.height.value;
                if (first_size < 0.0f || bar_size < 0.0f || second_size < 0.0f ||
                    !Near(first_size + bar_size + second_size, static_cast<float>(extent)))
                    return 8;
                if (candidate.constraints_satisfied && (first_size < 20.0f || second_size < 30.0f))
                    return 9;
            }
        }
    }

    return EXIT_SUCCESS;
}
