#include <mwfl/graphics.h>

int main() {
    mwfl::EnhancedMetafile metafile;
    mwfl::GdiPlusSession session;
    return metafile || session.IsStarted() ? 1 : 0;
}
