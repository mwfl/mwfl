#include <mwfl/d3d_host.h>

#include <vector>

class Direct3DFixture {
public:
    bool DrawOnDemand() {
        const mwfl::D3DFrameResult result = host_.RenderFrame();
        switch (result.status) {
            case mwfl::D3DFrameStatus::presented: return true;
            case mwfl::D3DFrameStatus::minimized:
            case mwfl::D3DFrameStatus::occluded:
            case mwfl::D3DFrameStatus::device_recreated: return false;
            case mwfl::D3DFrameStatus::failed: return false;
        }
        return false;
    }

private:
    mwfl::D3DHost host_;
    std::vector<float> device_independent_vertices_;
};
