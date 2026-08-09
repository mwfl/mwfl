#include <mwtl/imaging.h>

#include <cmath>

namespace {
bool Near(float left, float right) { return std::abs(left - right) < 0.01f; }
}

int main() {
    using namespace mwtl;
    ImageViewportModel viewport;
    if (viewport.Fit() || viewport.SetZoom(2, {}) || viewport.ZoomBy(0, {}) ||
        viewport.PanBy({Dip(NAN), 0.0_dip}))
        return 1;
    viewport.SetViewport({800.0_dip, 600.0_dip});
    viewport.SetImageSize(1600, 900);
    if (!viewport.Fit() || !Near(viewport.GetZoom(), 0.5f)) return 2;
    auto transform = viewport.GetTransform();
    if (!Near(transform.origin.x.value, 0) || !Near(transform.origin.y.value, 75) ||
        !Near(transform.rendered_size.width.value, 800))
        return 3;
    const PointDip anchor{200.0_dip, 200.0_dip};
    const float image_x = (anchor.x.value - transform.origin.x.value) / transform.scale;
    const float image_y = (anchor.y.value - transform.origin.y.value) / transform.scale;
    if (!viewport.ZoomBy(2, anchor)) return 4;
    transform = viewport.GetTransform();
    if (!Near((anchor.x.value - transform.origin.x.value) / transform.scale, image_x) ||
        !Near((anchor.y.value - transform.origin.y.value) / transform.scale, image_y))
        return 5;
    if (!viewport.PanBy({10000.0_dip, -10000.0_dip})) return 6;
    const PointDip pan = viewport.GetPan();
    if (!Near(pan.x.value, 400) || !Near(pan.y.value, -150)) return 7;
    if (!viewport.SetZoom(1000, anchor) || !Near(viewport.GetZoom(), 64)) return 8;
    viewport.SetImageSize(100, 100);
    if (!viewport.Fit() || !Near(viewport.GetZoom(), 1)) return 9;
    if (!viewport.Fit(true) || !Near(viewport.GetZoom(), 6)) return 10;
    return 0;
}
