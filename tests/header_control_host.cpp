#include <mwfl/control_host.h>

void mwfl_header_control_host_smoke() {
    mwfl::ControlHost host(nullptr);
    (void)host.GetParent();
}
