#include <mwtl/printing_settings.h>

int main() {
    const auto pages = mwtl::PaginateContent(21, 10);
    mwtl::PrintPreviewModel preview;
    preview.SetPageCount(pages.size());
    auto memory = mwtl::OwnedGlobalMemory::Allocate(64);
    return pages.size() == 3 && preview.SelectPage(2) && memory ? 0 : 1;
}
