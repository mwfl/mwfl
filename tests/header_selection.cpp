#include <mwfl/selection.h>

#include <mwfl/controls.h>

void selection_header_probe(mwfl::ComboBox& combo) {
    mwfl::SelectionAdapter<mwfl::ComboBox, int> items{combo};
    static_cast<void>(items.Size());
}
