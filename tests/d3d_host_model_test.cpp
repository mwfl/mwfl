#include <mwtl/d3d_host.h>

int main() {
    using namespace mwtl;
    D3DRenderStateModel model;
    if (model.GetState() != D3DRenderState::empty || model.BeginFrame()) return 1;
    if (!model.MarkReady() || !model.BeginFrame() || model.BeginFrame()) return 2;
    if (!model.EndFrame(S_OK) || model.GetState() != D3DRenderState::ready) return 3;
    if (!model.BeginFrame() || !model.EndFrame(DXGI_STATUS_OCCLUDED) ||
        model.GetState() != D3DRenderState::occluded)
        return 4;
    if (!model.MarkReady(true) || !model.UsesSoftwareAdapter()) return 5;
    if (!model.BeginFrame() || !model.EndFrame(DXGI_ERROR_DEVICE_REMOVED) ||
        model.GetState() != D3DRenderState::device_lost)
        return 6;
    model.MarkMinimized();
    if (model.GetState() != D3DRenderState::minimized) return 7;
    if (!model.MarkReady() || model.UsesSoftwareAdapter()) return 8;
    model.MarkDestroyed();
    if (model.MarkReady() || model.BeginFrame() || model.GetState() != D3DRenderState::destroyed)
        return 9;
    if (!IsD3DDeviceLoss(DXGI_ERROR_DEVICE_RESET) || IsD3DDeviceLoss(E_FAIL)) return 10;
    return 0;
}
