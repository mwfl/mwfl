#include <mwfl/d2d_host.h>

int main() {
    mwfl::D2DRenderStateModel model;
    return model.MarkReady() && model.GetState() == mwfl::D2DRenderState::ready ? 0 : 1;
}
