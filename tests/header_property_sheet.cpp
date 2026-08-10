#include <mwtl/property_sheet.h>

void CompilePropertySheetHeader() {
    mwtl::PropertySheetModel model;
    static_cast<void>(model.AnyDirty());
}
