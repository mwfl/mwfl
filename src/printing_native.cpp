#include <mwtl/printing_native.h>

#include <wingdi.h>

#include <algorithm>
#include <limits>
#include <utility>

namespace mwtl {
namespace detail {
PrintBackendResult AdoptPrinterDeviceContext(HDC dc);
}
namespace {

PrintOperationStatus MapError(DWORD error) noexcept {
    if (error == ERROR_CANCELLED || error == ERROR_PRINT_CANCELLED)
        return PrintOperationStatus::cancelled;
    if (error == ERROR_FILE_NOT_FOUND || error == ERROR_INVALID_PRINTER_NAME)
        return PrintOperationStatus::no_printer;
    if (error == ERROR_INVALID_DATA || error == ERROR_INVALID_PARAMETER)
        return PrintOperationStatus::invalid_settings;
    if (error == ERROR_INVALID_STATE) return PrintOperationStatus::invalid_settings;
    if (error == ERROR_ACCESS_DENIED || error == RPC_S_SERVER_UNAVAILABLE)
        return PrintOperationStatus::unavailable;
    return PrintOperationStatus::native_failure;
}

std::wstring NullTerminated(std::wstring_view value) {
    return std::wstring(value.begin(), value.end());
}

class HdcPrintBackend final : public PrintBackend {
public:
    explicit HdcPrintBackend(HDC dc) noexcept : dc_(dc) {}
    ~HdcPrintBackend() override {
        if (dc_) ::DeleteDC(dc_);
    }
    int StartDocument(std::wstring_view title) noexcept override {
        const std::wstring owned_title = NullTerminated(title);
        DOCINFOW info{sizeof(info)};
        info.lpszDocName = owned_title.c_str();
        const int result = ::StartDocW(dc_, &info);
        Capture(result);
        return result;
    }
    int StartPage() noexcept override {
        const int result = ::StartPage(dc_);
        Capture(result);
        return result;
    }
    int EndPage() noexcept override {
        const int result = ::EndPage(dc_);
        Capture(result);
        return result;
    }
    int EndDocument() noexcept override {
        const int result = ::EndDoc(dc_);
        Capture(result);
        return result;
    }
    int AbortDocument() noexcept override {
        const int result = ::AbortDoc(dc_);
        Capture(result);
        return result;
    }
    HDC DeviceContext() const noexcept override { return dc_; }
    DWORD LastError() const noexcept override { return error_; }

private:
    void Capture(int result) noexcept {
        error_ = result > 0 ? ERROR_SUCCESS : ::GetLastError();
        if (result <= 0 && error_ == ERROR_SUCCESS) error_ = ERROR_GEN_FAILURE;
    }
    HDC dc_ = nullptr;
    DWORD error_ = ERROR_SUCCESS;
};

std::int64_t PixelsToMicrons(int pixels, int dpi) noexcept {
    if (pixels <= 0 || dpi <= 0) return 0;
    return (static_cast<std::int64_t>(pixels) * 25'400 + dpi / 2) / dpi;
}

}  // namespace

bool PrinterCapabilities::IsValid() const noexcept {
    return dpi_x > 0 && dpi_y > 0 && printable_width_pixels > 0 &&
           printable_height_pixels > 0 && physical_width_pixels > 0 &&
           physical_height_pixels > 0 && physical_size.IsValid() && printable_size.IsValid();
}

PrinterCapabilitiesResult QueryPrinterCapabilities(std::wstring_view printer_name) {
    PrinterCapabilitiesResult result;
    if (printer_name.empty()) {
        result.status = PrintOperationStatus::invalid_settings;
        result.error = ERROR_INVALID_PARAMETER;
        return result;
    }
    const std::wstring name = NullTerminated(printer_name);
    HDC dc = ::CreateICW(L"WINSPOOL", name.c_str(), nullptr, nullptr);
    if (!dc) {
        result.error = ::GetLastError();
        if (result.error == ERROR_SUCCESS) result.error = ERROR_GEN_FAILURE;
        result.status = MapError(result.error);
        return result;
    }
    auto& value = result.capabilities;
    value.dpi_x = ::GetDeviceCaps(dc, LOGPIXELSX);
    value.dpi_y = ::GetDeviceCaps(dc, LOGPIXELSY);
    value.printable_width_pixels = ::GetDeviceCaps(dc, HORZRES);
    value.printable_height_pixels = ::GetDeviceCaps(dc, VERTRES);
    value.physical_width_pixels = ::GetDeviceCaps(dc, PHYSICALWIDTH);
    value.physical_height_pixels = ::GetDeviceCaps(dc, PHYSICALHEIGHT);
    value.offset_x_pixels = ::GetDeviceCaps(dc, PHYSICALOFFSETX);
    value.offset_y_pixels = ::GetDeviceCaps(dc, PHYSICALOFFSETY);
    ::DeleteDC(dc);
    value.physical_size = {PixelsToMicrons(value.physical_width_pixels, value.dpi_x),
                           PixelsToMicrons(value.physical_height_pixels, value.dpi_y)};
    value.printable_size = {PixelsToMicrons(value.printable_width_pixels, value.dpi_x),
                            PixelsToMicrons(value.printable_height_pixels, value.dpi_y)};
    if (!value.IsValid()) {
        result.status = PrintOperationStatus::invalid_settings;
        result.error = ERROR_INVALID_DATA;
        return result;
    }
    result.status = PrintOperationStatus::success;
    result.error = ERROR_SUCCESS;
    return result;
}

PrintBackendResult CreatePrinterBackend(std::wstring_view printer_name) {
    PrintBackendResult result;
    if (printer_name.empty()) {
        result.status = PrintOperationStatus::invalid_settings;
        result.error = ERROR_INVALID_PARAMETER;
        return result;
    }
    const std::wstring name = NullTerminated(printer_name);
    HDC dc = ::CreateDCW(L"WINSPOOL", name.c_str(), nullptr, nullptr);
    if (!dc) {
        result.error = ::GetLastError();
        if (result.error == ERROR_SUCCESS) result.error = ERROR_GEN_FAILURE;
        result.status = MapError(result.error);
        return result;
    }
    return detail::AdoptPrinterDeviceContext(dc);
}

namespace detail {
PrintBackendResult AdoptPrinterDeviceContext(HDC dc) {
    PrintBackendResult result;
    if (!dc) {
        result.error = ERROR_INVALID_HANDLE;
        result.status = PrintOperationStatus::native_failure;
        return result;
    }
    result.backend = std::make_unique<HdcPrintBackend>(dc);
    result.status = PrintOperationStatus::success;
    result.error = ERROR_SUCCESS;
    return result;
}
}  // namespace detail

PrintJob::PrintJob(std::unique_ptr<PrintBackend> backend) noexcept
    : backend_(std::move(backend)) {}

PrintJob::~PrintJob() {
    if (transaction_.GetState() == PrintTransactionState::document ||
        transaction_.GetState() == PrintTransactionState::page) {
        if (backend_) backend_->AbortDocument();
        transaction_.Abort();
    }
}

PrintOperationStatus PrintJob::Fail(DWORD error) noexcept {
    last_error_ = error == ERROR_SUCCESS ? ERROR_GEN_FAILURE : error;
    if (backend_ && (transaction_.GetState() == PrintTransactionState::document ||
                     transaction_.GetState() == PrintTransactionState::page))
        backend_->AbortDocument();
    transaction_.Fail();
    return MapError(last_error_);
}

PrintOperationStatus PrintJob::Begin(std::wstring_view title) noexcept {
    if (!backend_ || title.empty() || transaction_.GetState() != PrintTransactionState::idle)
        return Fail(ERROR_INVALID_PARAMETER);
    if (backend_->StartDocument(title) <= 0) return Fail(backend_->LastError());
    transaction_.BeginDocument();
    return PrintOperationStatus::success;
}

PrintOperationStatus PrintJob::BeginPage() noexcept {
    if (!backend_ || transaction_.GetState() != PrintTransactionState::document)
        return Fail(ERROR_INVALID_STATE);
    if (backend_->StartPage() <= 0) return Fail(backend_->LastError());
    transaction_.BeginPage();
    return PrintOperationStatus::success;
}

PrintOperationStatus PrintJob::EndPage() noexcept {
    if (!backend_ || transaction_.GetState() != PrintTransactionState::page)
        return Fail(ERROR_INVALID_STATE);
    if (backend_->EndPage() <= 0) return Fail(backend_->LastError());
    transaction_.EndPage();
    return PrintOperationStatus::success;
}

PrintOperationStatus PrintJob::Complete() noexcept {
    if (!backend_ || transaction_.GetState() != PrintTransactionState::document)
        return Fail(ERROR_INVALID_STATE);
    if (backend_->EndDocument() <= 0) return Fail(backend_->LastError());
    transaction_.Complete();
    return PrintOperationStatus::success;
}

PrintOperationStatus PrintJob::Cancel() noexcept {
    const auto state = transaction_.GetState();
    if (!backend_ || (state != PrintTransactionState::document &&
                      state != PrintTransactionState::page)) {
        last_error_ = ERROR_INVALID_STATE;
        return PrintOperationStatus::invalid_settings;
    }
    const int native_result = backend_->AbortDocument();
    transaction_.Abort();
    if (native_result <= 0) {
        last_error_ = backend_->LastError();
        return MapError(last_error_);
    }
    last_error_ = ERROR_CANCELLED;
    return PrintOperationStatus::cancelled;
}

HDC PrintJob::DeviceContext() const noexcept {
    return backend_ ? backend_->DeviceContext() : nullptr;
}

PrintOperationStatus PrintPages(PrintJob& job, std::wstring_view title,
                                std::span<const PrintPage> pages,
                                const PrintPageRenderer& renderer) noexcept {
    if (pages.empty() || !renderer) return PrintOperationStatus::invalid_settings;
    auto status = job.Begin(title);
    if (status != PrintOperationStatus::success) return status;
    for (const auto& page : pages) {
        status = job.BeginPage();
        if (status != PrintOperationStatus::success) return status;
        try {
            if (!renderer(job.DeviceContext(), page)) {
                job.Cancel();
                return PrintOperationStatus::render_failure;
            }
        } catch (...) {
            job.Cancel();
            return PrintOperationStatus::render_exception;
        }
        status = job.EndPage();
        if (status != PrintOperationStatus::success) return status;
    }
    return job.Complete();
}

}  // namespace mwtl
