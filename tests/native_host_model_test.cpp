#include <mwtl/native_host.h>

int main() {
    using mwtl::NativeHostState;
    using mwtl::NativeHostStateModel;

    NativeHostStateModel model;
    if (model.GetState() != NativeHostState::uninitialized || model.IsAttached() ||
        model.Attach(1) || model.Detach() != 0)
        return 1;
    if (!model.MarkCreated() || model.MarkCreated() ||
        model.GetState() != NativeHostState::ready || model.Attach(0))
        return 2;
    if (!model.Attach(42) || !model.IsAttached() || model.GetAttachment() != 42 ||
        model.Attach(43) || model.MarkCreated())
        return 3;
    if (model.Detach() != 42 || model.GetState() != NativeHostState::ready ||
        model.Detach() != 0)
        return 4;
    if (!model.Attach(77)) return 5;
    model.MarkDestroyed();
    if (model.GetState() != NativeHostState::destroyed || model.IsAttached() ||
        model.GetAttachment() != 0 || model.Attach(1) || model.Detach() != 0)
        return 6;
    if (!model.MarkCreated() || model.GetState() != NativeHostState::ready) return 7;
    return 0;
}
