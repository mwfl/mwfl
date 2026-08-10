#include <mwfl/property_sheet.h>

void CompilePropertySheetHeader() {
    mwfl::PropertySheetModel model;
    static_cast<void>(model.AnyDirty());
}
