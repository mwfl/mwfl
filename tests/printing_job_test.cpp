#include <mwfl/printing_native.h>

#include <memory>
#include <stdexcept>
#include <string_view>

namespace {
struct Counts { int start_doc=0, start_page=0, end_page=0, end_doc=0, abort_doc=0; };

class FakeBackend final : public mwfl::PrintBackend {
public:
    FakeBackend(Counts& counts, int fail_at = 0) : counts_(counts), fail_at_(fail_at) {}
    int StartDocument(std::wstring_view) noexcept override { return Call(counts_.start_doc); }
    int StartPage() noexcept override { return Call(counts_.start_page); }
    int EndPage() noexcept override { return Call(counts_.end_page); }
    int EndDocument() noexcept override { return Call(counts_.end_doc); }
    int AbortDocument() noexcept override { ++counts_.abort_doc; return 1; }
    HDC DeviceContext() const noexcept override { return reinterpret_cast<HDC>(1); }
    DWORD LastError() const noexcept override { return error_; }
private:
    int Call(int& count) noexcept {
        ++count;
        ++calls_;
        if (calls_ == fail_at_) { error_ = ERROR_PRINT_CANCELLED; return 0; }
        return 1;
    }
    Counts& counts_;
    int fail_at_ = 0;
    int calls_ = 0;
    DWORD error_ = ERROR_SUCCESS;
};
}

int main() {
    using namespace mwfl;
    const auto pages = PaginateContent(25, 10);
    Counts success;
    {
        PrintJob job(std::make_unique<FakeBackend>(success));
        const auto result = PrintPages(job, L"test", pages,
            [](HDC dc, const PrintPage&) { return dc != nullptr; });
        if (result != PrintOperationStatus::success || success.start_doc != 1 ||
            success.start_page != 3 || success.end_page != 3 || success.end_doc != 1 ||
            success.abort_doc != 0 || job.Transaction().GetCompletedPageCount() != 3)
            return 1;
    }
    Counts rejected;
    {
        PrintJob job(std::make_unique<FakeBackend>(rejected));
        if (PrintPages(job, L"test", pages, [](HDC, const PrintPage&) { return false; }) !=
                PrintOperationStatus::render_failure || rejected.abort_doc != 1 ||
            rejected.end_page != 0 || rejected.end_doc != 0)
            return 2;
    }
    Counts exception;
    {
        PrintJob job(std::make_unique<FakeBackend>(exception));
        if (PrintPages(job, L"test", pages, [](HDC, const PrintPage&) -> bool {
                throw std::runtime_error("render");
            }) != PrintOperationStatus::render_exception || exception.abort_doc != 1)
            return 3;
    }
    Counts native_failure;
    {
        PrintJob job(std::make_unique<FakeBackend>(native_failure, 3));
        if (PrintPages(job, L"test", pages, [](HDC, const PrintPage&) { return true; }) !=
                PrintOperationStatus::cancelled || native_failure.abort_doc != 1 ||
            native_failure.end_doc != 0)
            return 4;
    }
    for (int failed_call = 1; failed_call <= 5; ++failed_call) {
        Counts failure;
        PrintJob job(std::make_unique<FakeBackend>(failure, failed_call));
        const auto status = PrintPages(job, L"failure matrix", pages,
            [](HDC, const PrintPage&) { return true; });
        const int expected_aborts = failed_call == 1 ? 0 : 1;
        if (status != PrintOperationStatus::cancelled ||
            failure.abort_doc != expected_aborts ||
            failure.end_doc > 1) return 8 + failed_call;
    }
    Counts explicit_cancel;
    {
        PrintJob job(std::make_unique<FakeBackend>(explicit_cancel));
        if (job.Begin(L"cancel") != PrintOperationStatus::success ||
            job.BeginPage() != PrintOperationStatus::success ||
            job.Cancel() != PrintOperationStatus::cancelled ||
            job.Cancel() != PrintOperationStatus::invalid_settings ||
            explicit_cancel.abort_doc != 1 ||
            explicit_cancel.end_page != 0 || explicit_cancel.end_doc != 0) return 14;
    }
    Counts predicate_cancel;
    {
        PrintJob job(std::make_unique<FakeBackend>(predicate_cancel));
        int rendered = 0;
        const auto status = PrintPages(job, L"predicate cancel", pages,
            [&](HDC, const PrintPage&) { ++rendered; return true; },
            [](const PrintPage& page) { return page.index == 1; });
        if (status != PrintOperationStatus::cancelled || rendered != 1 ||
            predicate_cancel.start_page != 1 || predicate_cancel.end_page != 1 ||
            predicate_cancel.abort_doc != 1 || predicate_cancel.end_doc != 0) return 15;
    }
    Counts predicate_exception;
    {
        PrintJob job(std::make_unique<FakeBackend>(predicate_exception));
        const auto status = PrintPages(job, L"predicate exception", pages,
            [](HDC, const PrintPage&) { return true; },
            [](const PrintPage&) -> bool { throw std::runtime_error("cancel check"); });
        if (status != PrintOperationStatus::render_exception ||
            predicate_exception.start_page != 0 || predicate_exception.abort_doc != 1)
            return 16;
    }
    Counts teardown;
    {
        PrintJob job(std::make_unique<FakeBackend>(teardown));
        if (job.Begin(L"test") != PrintOperationStatus::success ||
            job.BeginPage() != PrintOperationStatus::success)
            return 5;
    }
    if (teardown.abort_doc != 1) return 6;
    if (QueryPrinterCapabilities(L"").status != PrintOperationStatus::invalid_settings ||
        CreatePrinterBackend(L"").status != PrintOperationStatus::invalid_settings)
        return 7;
    return 0;
}
