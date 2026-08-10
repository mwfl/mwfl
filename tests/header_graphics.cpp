#include <mwtl/graphics.h>

#include <type_traits>

static_assert(!std::is_copy_constructible_v<mwtl::EnhancedMetafile>);
