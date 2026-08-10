#include <mwfl/tray_icon.h>

void ConsumeTrayIconHeader() {
    mwfl::TrayIcon icon;
    mwfl::TrayIconOptions options;
    static_cast<void>(icon.GetState());
    static_cast<void>(options.id);
}
