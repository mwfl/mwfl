#include <mwtl/async.h>

#include <type_traits>

static_assert(!std::is_copy_constructible_v<mwtl::UniqueSubscription>);
static_assert(std::is_move_constructible_v<mwtl::UniqueSubscription>);
static_assert(!std::is_move_constructible_v<mwtl::AsyncInitialization>);
