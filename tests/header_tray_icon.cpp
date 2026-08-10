#include <mwtl/tray_icon.h>

void ConsumeTrayIconHeader() {
    mwtl::TrayIcon icon;
    mwtl::TrayIconOptions options;
    static_cast<void>(icon.GetState());
    static_cast<void>(options.id);
}
