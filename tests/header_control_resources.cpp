#include <mwfl/control_resources.h>

void ConsumeControlResources() {
    mwfl::ImageList images;
    mwfl::Tooltip tooltip;
    mwfl::TaskDialogResult result;
    mwfl::TaskDialogOptions options;
    mwfl::TaskDialogController controller{nullptr};
    static_cast<void>(images); static_cast<void>(tooltip); static_cast<void>(result);
    static_cast<void>(options); static_cast<void>(controller);
}
