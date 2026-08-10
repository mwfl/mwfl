#include <mwtl/control_resources.h>

void ConsumeControlResources() {
    mwtl::ImageList images;
    mwtl::Tooltip tooltip;
    mwtl::TaskDialogResult result;
    mwtl::TaskDialogOptions options;
    mwtl::TaskDialogController controller{nullptr};
    static_cast<void>(images); static_cast<void>(tooltip); static_cast<void>(result);
    static_cast<void>(options); static_cast<void>(controller);
}
