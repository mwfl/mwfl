#include <mwtl/dialog.h>

void ConsumeDialogHeader() {
    mwtl::DialogOptions options;
    mwtl::Dialog dialog{std::move(options)};
    static_cast<void>(dialog);
}
