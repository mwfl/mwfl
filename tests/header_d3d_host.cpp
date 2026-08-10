#include <mwtl/d3d_host.h>

#include <type_traits>

static_assert(!std::is_move_constructible_v<mwtl::D3DHost>);
