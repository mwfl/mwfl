#include <mwfl/scintilla.h>

#include <type_traits>

static_assert(!std::is_copy_constructible_v<mwfl::ScintillaRuntime>);
static_assert(!std::is_move_constructible_v<mwfl::ScintillaEditor>);
static_assert(mwfl::kScintillaVersion == "5.6.5");
