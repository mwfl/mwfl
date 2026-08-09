#include <mwtl/native_host.h>

void mwtl_header_native_host_probe() {
    mwtl::NativeHostStateModel state;
    static_cast<void>(state.GetState());
}
