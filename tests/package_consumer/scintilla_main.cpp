#include <mwfl/scintilla.h>

int main() {
    mwfl::ScintillaRuntime runtime;
    const auto text = mwfl::FromUtf8("installed");
    return runtime.LoadAdjacent() && text && *text == L"installed" ? 0 : 1;
}
