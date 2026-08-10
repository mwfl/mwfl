#include <mwfl/d3d_host.h>

int main() {
    mwfl::D3DRenderStateModel model;
    return model.MarkReady() && model.GetState() == mwfl::D3DRenderState::ready ? 0 : 1;
}
