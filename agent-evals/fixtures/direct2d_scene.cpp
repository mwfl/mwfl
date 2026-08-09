#include <mwtl/d2d_host.h>

#include <wrl/client.h>

#include <vector>

struct ScenePoint { mwtl::PointDip position; };

class Direct2DFixture {
public:
    mwtl::D2DHostOptions Options() {
        mwtl::D2DHostOptions options;
        options.callbacks.create_device_resources = [this](ID2D1HwndRenderTarget& target) {
            static_cast<void>(target.CreateSolidColorBrush(
                D2D1::ColorF(D2D1::ColorF::RoyalBlue), brush_.ReleaseAndGetAddressOf()));
        };
        options.callbacks.discard_device_resources = [this] { brush_.Reset(); };
        options.callbacks.paint = [this](mwtl::D2DRenderContext& context) {
            if (brush_ && !points_.empty()) {
                const auto point = points_.front().position;
                context.target.FillEllipse(D2D1::Ellipse({point.x.value, point.y.value}, 4, 4),
                                           brush_.Get());
            }
        };
        return options;
    }

private:
    std::vector<ScenePoint> points_;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush_;
};
