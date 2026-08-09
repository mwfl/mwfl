#include <mwtl/d2d_host.h>

int main() {
    using namespace mwtl;
    D2DRenderStateModel model;
    if (model.GetState() != D2DRenderState::empty || model.BeginDraw()) return 1;
    if (!model.MarkReady() || !model.BeginDraw() || model.BeginDraw()) return 2;
    if (!model.EndDraw(S_OK) || model.GetState() != D2DRenderState::ready) return 3;
    if (!model.BeginDraw() || !model.EndDraw(D2DERR_RECREATE_TARGET) ||
        model.GetState() != D2DRenderState::device_lost)
        return 4;
    if (!model.MarkReady() || model.GetState() != D2DRenderState::ready) return 5;
    model.MarkDeviceLost();
    if (model.GetState() != D2DRenderState::device_lost) return 6;
    model.MarkDestroyed();
    if (model.MarkReady() || model.BeginDraw() || model.GetState() != D2DRenderState::destroyed)
        return 7;
    return 0;
}
