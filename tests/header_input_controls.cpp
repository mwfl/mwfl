#include <mwfl/input_controls.h>

void ConsumeInputControls() {
    mwfl::DateTimePicker date;
    mwfl::MonthCalendar calendar;
    mwfl::HotKey hot_key;
    mwfl::IpAddress address;
    mwfl::UpDown spin;
    mwfl::SysLink link;
    static_cast<void>(date); static_cast<void>(calendar); static_cast<void>(hot_key);
    static_cast<void>(address); static_cast<void>(spin); static_cast<void>(link);
}
