#include <mwfl/printing_native.h>

#include <span>
#include <string_view>

mwfl::PrintOperationStatus PrintFixture(mwfl::PrintJob& job,
                                        std::span<const mwfl::PrintPage> pages) {
    return mwfl::PrintPages(job, L"Agent fixture", pages,
                            [](HDC dc, const mwfl::PrintPage& page) {
                                return dc != nullptr && page.content_end >= page.content_begin;
                            });
}
