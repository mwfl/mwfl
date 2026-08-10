#include <mwfl/printing.h>

#include <cmath>
#include <limits>

int main() {
    using namespace mwfl;
    constexpr PrintLayout a4{{210000, 297000}, {10000, 12000, 10000, 12000}};
    static_assert(a4.IsValid());
    static_assert(a4.ContentExtent().width == 190000);
    static_assert(!PrintLayout{{100, 100}, {50, 0, 50, 0}}.IsValid());

    if (!PaginateContent(0, 10).empty() || !PaginateContent(10, 0).empty()) return 1;
    const auto pages = PaginateContent(25, 10);
    if (pages.size() != 3 || pages[0].id.value != 1 || pages[1].content_begin != 10 ||
        pages[2].content_end != 25)
        return 2;
    if (!PrintPageRange{0, 2}.IsValidFor(pages.size()) ||
        PrintPageRange{2, 1}.IsValidFor(pages.size()) ||
        PrintPageRange{0, 3}.IsValidFor(pages.size()))
        return 3;

    PrintPreviewModel preview;
    if (preview.SelectPage(0) || preview.MoveBy(1)) return 4;
    preview.SetPageCount(3);
    if (!preview.SelectPage(2) || preview.MoveBy(1) || !preview.MoveBy(-1) ||
        preview.GetSelectedPage() != 1)
        return 5;
    if (!preview.MoveBy((std::numeric_limits<std::ptrdiff_t>::min)()) ||
        preview.GetSelectedPage() != 0 ||
        !preview.MoveBy((std::numeric_limits<std::ptrdiff_t>::max)()) ||
        preview.GetSelectedPage() != 2)
        return 11;
    if (preview.SetZoom(std::numeric_limits<double>::quiet_NaN()) || preview.SetZoom(100) ||
        !preview.SetZoom(2.0) || preview.GetScaleMode() != PrintPreviewScaleMode::custom)
        return 6;
    preview.SetPageCount(1);
    if (preview.GetSelectedPage() != 0) return 7;

    PrintTransactionModel job;
    if (job.BeginPage() || job.Complete() || !job.BeginDocument() || job.BeginDocument() ||
        !job.BeginPage() || job.Complete() || !job.EndPage() || !job.Complete() ||
        job.GetCompletedPageCount() != 1 || job.Abort())
        return 8;
    PrintTransactionModel cancelled;
    if (!cancelled.BeginDocument() || !cancelled.BeginPage() || !cancelled.Abort() ||
        cancelled.GetState() != PrintTransactionState::aborted)
        return 9;
    PrintTransactionModel failed;
    failed.Fail();
    if (failed.GetState() != PrintTransactionState::failed || failed.BeginDocument()) return 10;
    return 0;
}
