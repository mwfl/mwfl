#include <mwfl/command_controls.h>

void ConsumeCommandControls() {
    mwfl::Toolbar toolbar; mwfl::StatusBar status; mwfl::Rebar rebar;
    mwfl::Pager pager; mwfl::Animation animation;
    static_cast<void>(toolbar); static_cast<void>(status); static_cast<void>(rebar);
    static_cast<void>(pager); static_cast<void>(animation);
}
