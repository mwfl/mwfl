#include <mwfl/concepts.h>

struct ConceptWindow {
    HWND GetHwnd() const noexcept { return nullptr; }
};

static_assert(mwfl::WindowLike<ConceptWindow>);
