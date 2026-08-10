#include <mwfl/d2d_host.h>

#include <type_traits>

static_assert(!std::is_move_constructible_v<mwfl::D2DHost>);
