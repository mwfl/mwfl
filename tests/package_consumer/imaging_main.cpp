#include <mwtl/imaging.h>

using mwtl::operator""_dip;

int main() {
    mwtl::ImageViewportModel viewport;
    viewport.SetImageSize(100, 100);
    viewport.SetViewport({100.0_dip, 100.0_dip});
    return viewport.Fit() ? 0 : 1;
}
