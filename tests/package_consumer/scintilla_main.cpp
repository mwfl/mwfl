#include <mwtl/scintilla.h>

int main() {
    mwtl::ScintillaRuntime runtime;
    const auto text = mwtl::FromUtf8("installed");
    return runtime.LoadAdjacent() && text && *text == L"installed" ? 0 : 1;
}
