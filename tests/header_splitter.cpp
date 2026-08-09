#include <mwtl/splitter.h>

void CompileSplitterHeader() {
    mwtl::SplitterModel splitter;
    static_cast<void>(splitter.Arrange(mwtl::SizeDip{}));
}
