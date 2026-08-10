#include <mwfl/native_host.h>

void mwfl_header_native_host_probe() {
    mwfl::NativeHostStateModel state;
    static_cast<void>(state.GetState());
}
