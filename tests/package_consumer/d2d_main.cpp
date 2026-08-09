#include <mwtl/d2d_host.h>

int main() {
    mwtl::D2DRenderStateModel model;
    return model.MarkReady() && model.GetState() == mwtl::D2DRenderState::ready ? 0 : 1;
}
