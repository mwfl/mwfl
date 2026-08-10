#include <mwfl/binding.h>

void HeaderBindingCompiles() {
    mwfl::ChangeGate gate;
    static_cast<void>(gate);
}
