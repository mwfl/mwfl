#include <mwfl/graphics.h>

#include <type_traits>

static_assert(!std::is_copy_constructible_v<mwfl::EnhancedMetafile>);
