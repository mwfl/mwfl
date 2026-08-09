#include <mwtl/scintilla.h>

#include <type_traits>

static_assert(!std::is_copy_constructible_v<mwtl::ScintillaRuntime>);
static_assert(!std::is_move_constructible_v<mwtl::ScintillaEditor>);
static_assert(mwtl::kScintillaVersion == "5.6.5");
