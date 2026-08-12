#include <mwfl/appearance.h>

void HeaderAppearanceCompiles() {
    mwfl::AppearanceOptions options;
    mwfl::AppearanceState state = mwfl::ResolveAppearance(options);
    static_cast<void>(state.IsDark());
    static_cast<void>(mwfl::GetWindowAppearance(nullptr));
    static_cast<void>(options);
}
