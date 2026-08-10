#pragma once

#ifdef MWFL_TESTING

namespace mwfl::detail {

struct LifecycleSnapshot {
    int module_initialized;
    int loop_registered;
    int loop_removed;
    int module_terminated;
};

void ResetLifecycleSnapshotForTesting() noexcept;
LifecycleSnapshot GetLifecycleSnapshotForTesting() noexcept;

}  // namespace mwfl::detail

#endif
