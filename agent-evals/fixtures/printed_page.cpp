#include <mwtl/printing_native.h>

#include <span>
#include <string_view>

mwtl::PrintOperationStatus PrintFixture(mwtl::PrintJob& job,
                                        std::span<const mwtl::PrintPage> pages) {
    return mwtl::PrintPages(job, L"Agent fixture", pages,
                            [](HDC dc, const mwtl::PrintPage& page) {
                                return dc != nullptr && page.content_end >= page.content_begin;
                            });
}
