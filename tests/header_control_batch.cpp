#include <mwfl/control_batch.h>

void mwfl_header_control_batch_smoke() {
    mwfl::ComboBox combo;
    const auto result = mwfl::AddItems(combo, {L"one", L"two"});
    static_cast<void>(result);
}
