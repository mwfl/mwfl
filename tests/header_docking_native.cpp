#include <mwtl/docking_native.h>

#include <type_traits>

static_assert(!std::is_copy_constructible_v<mwtl::DockNativeWorkspaceAdapter>);
