#include <mwfl/printing.h>

#include <vector>

struct PreviewFixture {
    std::vector<mwfl::PrintPage> pages = mwfl::PaginateContent(120, 32);
    mwfl::PrintPreviewModel preview;

    PreviewFixture() { preview.SetPageCount(pages.size()); }
    bool Next() { return preview.MoveBy(1); }
    bool Zoom(double value) { return preview.SetZoom(value); }
};
