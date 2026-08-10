#include <mwtl/d3d_host.h>

#include <vector>

class Direct3DFixture {
public:
    bool DrawOnDemand() {
        const mwtl::D3DFrameResult result = host_.RenderFrame();
        switch (result.status) {
            case mwtl::D3DFrameStatus::presented: return true;
            case mwtl::D3DFrameStatus::minimized:
            case mwtl::D3DFrameStatus::occluded:
            case mwtl::D3DFrameStatus::device_recreated: return false;
            case mwtl::D3DFrameStatus::failed: return false;
        }
        return false;
    }

private:
    mwtl::D3DHost host_;
    std::vector<float> device_independent_vertices_;
};
