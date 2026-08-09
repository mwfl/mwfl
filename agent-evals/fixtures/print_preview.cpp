#include <mwtl/printing.h>

#include <vector>

struct PreviewFixture {
    std::vector<mwtl::PrintPage> pages = mwtl::PaginateContent(120, 32);
    mwtl::PrintPreviewModel preview;

    PreviewFixture() { preview.SetPageCount(pages.size()); }
    bool Next() { return preview.MoveBy(1); }
    bool Zoom(double value) { return preview.SetZoom(value); }
};
