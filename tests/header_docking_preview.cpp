#include <mwtl/docking_preview.h>

#include <type_traits>

static_assert(!std::is_copy_constructible_v<mwtl::DockPreviewWindow>);
