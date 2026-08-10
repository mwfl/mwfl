#include <mwtl/graphics.h>

int main() {
    mwtl::EnhancedMetafile metafile;
    mwtl::GdiPlusSession session;
    return metafile || session.IsStarted() ? 1 : 0;
}
