#include <mwtl/selection.h>

#include <mwtl/controls.h>

void selection_header_probe(mwtl::ComboBox& combo) {
    mwtl::SelectionAdapter<mwtl::ComboBox, int> items{combo};
    static_cast<void>(items.Size());
}
