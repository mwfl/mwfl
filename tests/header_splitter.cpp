#include <mwfl/splitter.h>

void CompileSplitterHeader() {
    mwfl::SplitterModel splitter;
    static_cast<void>(splitter.Arrange(mwfl::SizeDip{}));
}
