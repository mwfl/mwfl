#pragma once

#include <windows.h>

#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>

#include <mwtl/printing.h>

namespace mwtl {

enum class PrintOperationStatus {
    success,
    cancelled,
    no_printer,
    invalid_settings,
    unavailable,
    native_failure,
    render_failure,
    render_exception,
};

struct PrinterCapabilities {
    int dpi_x = 0;
    int dpi_y = 0;
    int printable_width_pixels = 0;
    int printable_height_pixels = 0;
    int physical_width_pixels = 0;
    int physical_height_pixels = 0;
    int offset_x_pixels = 0;
    int offset_y_pixels = 0;
    PrintExtentMicrons physical_size{};
    PrintExtentMicrons printable_size{};
    bool IsValid() const noexcept;
};

struct PrinterCapabilitiesResult {
    PrinterCapabilities capabilities{};
    PrintOperationStatus status = PrintOperationStatus::native_failure;
    DWORD error = ERROR_GEN_FAILURE;
    explicit operator bool() const noexcept { return status == PrintOperationStatus::success; }
};

// Creates and destroys an information DC synchronously. No printer handle or DC
// escapes. Device pixels and physical micrometres remain distinct in the result.
PrinterCapabilitiesResult QueryPrinterCapabilities(std::wstring_view printer_name);

// Backend contract for native printing and deterministic tests. Implementations
// must not throw. DeviceContext() is borrowed and valid only while the backend is.
class PrintBackend {
public:
    virtual ~PrintBackend() = default;
    virtual int StartDocument(std::wstring_view title) noexcept = 0;
    virtual int StartPage() noexcept = 0;
    virtual int EndPage() noexcept = 0;
    virtual int EndDocument() noexcept = 0;
    virtual int AbortDocument() noexcept = 0;
    virtual HDC DeviceContext() const noexcept = 0;
    virtual DWORD LastError() const noexcept = 0;
};

struct PrintBackendResult {
    std::unique_ptr<PrintBackend> backend;
    PrintOperationStatus status = PrintOperationStatus::native_failure;
    DWORD error = ERROR_GEN_FAILURE;
    explicit operator bool() const noexcept { return backend != nullptr; }
};

// Uses the printer's current default DEVMODE. The created backend owns its HDC.
PrintBackendResult CreatePrinterBackend(std::wstring_view printer_name);

class PrintJob final {
public:
    explicit PrintJob(std::unique_ptr<PrintBackend> backend) noexcept;
    ~PrintJob();
    PrintJob(const PrintJob&) = delete;
    PrintJob& operator=(const PrintJob&) = delete;
    PrintJob(PrintJob&&) = delete;
    PrintJob& operator=(PrintJob&&) = delete;

    PrintOperationStatus Begin(std::wstring_view title) noexcept;
    PrintOperationStatus BeginPage() noexcept;
    PrintOperationStatus EndPage() noexcept;
    PrintOperationStatus Complete() noexcept;
    PrintOperationStatus Cancel() noexcept;
    HDC DeviceContext() const noexcept;
    DWORD LastError() const noexcept { return last_error_; }
    const PrintTransactionModel& Transaction() const noexcept { return transaction_; }

private:
    PrintOperationStatus Fail(DWORD error) noexcept;
    std::unique_ptr<PrintBackend> backend_;
    PrintTransactionModel transaction_;
    DWORD last_error_ = ERROR_SUCCESS;
};

using PrintPageRenderer = std::function<bool(HDC, const PrintPage&)>;
using PrintCancellationCheck = std::function<bool(const PrintPage&)>;

// Renders exactly the supplied pages. False, exceptions, or any native failure
// abort the document. should_cancel is called before each page; true aborts and
// returns cancelled. Callback exceptions are contained as render_exception.
// A successful return guarantees EndDoc completed.
PrintOperationStatus PrintPages(PrintJob& job, std::wstring_view title,
                                std::span<const PrintPage> pages,
                                const PrintPageRenderer& renderer,
                                const PrintCancellationCheck& should_cancel = {}) noexcept;

}  // namespace mwtl
