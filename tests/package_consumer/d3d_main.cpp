#include <mwtl/d3d_host.h>

int main() {
    mwtl::D3DRenderStateModel model;
    return model.MarkReady() && model.GetState() == mwtl::D3DRenderState::ready ? 0 : 1;
}
