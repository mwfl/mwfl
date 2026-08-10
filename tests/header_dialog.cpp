#include <mwfl/dialog.h>

void ConsumeDialogHeader() {
    mwfl::DialogOptions options;
    mwfl::Dialog dialog{std::move(options)};
    static_cast<void>(dialog);
}
