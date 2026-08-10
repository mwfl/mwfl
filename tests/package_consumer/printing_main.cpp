#include <mwfl/printing_settings.h>

int main() {
    const auto pages = mwfl::PaginateContent(21, 10);
    mwfl::PrintPreviewModel preview;
    preview.SetPageCount(pages.size());
    auto memory = mwfl::OwnedGlobalMemory::Allocate(64);
    return pages.size() == 3 && preview.SelectPage(2) && memory ? 0 : 1;
}
