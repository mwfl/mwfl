#include <mwtl/printing.h>

#include <winspool.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace mwtl {
namespace {

std::wstring Copy(const wchar_t* value) { return value ? value : L""; }

PrinterQueryStatus MapPrinterError(DWORD error) noexcept {
    if (error == ERROR_FILE_NOT_FOUND || error == ERROR_INVALID_NAME)
        return PrinterQueryStatus::no_default_printer;
    if (error == ERROR_INVALID_DATA) return PrinterQueryStatus::invalid_data;
    if (error == ERROR_ACCESS_DENIED || error == RPC_S_SERVER_UNAVAILABLE)
        return PrinterQueryStatus::unavailable;
    return PrinterQueryStatus::failed;
}

}  // namespace

std::vector<PrintPage> PaginateContent(std::int64_t content_length,
                                       std::int64_t units_per_page) {
    std::vector<PrintPage> pages;
    if (content_length <= 0 || units_per_page <= 0) return pages;
    const auto count = 1 + (content_length - 1) / units_per_page;
    if (static_cast<std::uint64_t>(count) >
        static_cast<std::uint64_t>((std::numeric_limits<std::size_t>::max)()))
        return pages;
    pages.reserve(static_cast<std::size_t>(count));
    for (std::int64_t index = 0; index < count; ++index) {
        const auto begin = index * units_per_page;
        pages.push_back({PrintPageId{static_cast<std::uint64_t>(index + 1)},
                         static_cast<std::size_t>(index), begin,
                         (std::min)(content_length, begin + units_per_page)});
    }
    return pages;
}

bool PrintPageRange::IsValidFor(std::size_t page_count) const noexcept {
    return page_count > 0 && first <= last && last < page_count;
}

void PrintPreviewModel::SetPageCount(std::size_t count) noexcept {
    page_count_ = count;
    if (count == 0) selected_page_ = 0;
    else if (selected_page_ >= count) selected_page_ = count - 1;
}

bool PrintPreviewModel::SelectPage(std::size_t index) noexcept {
    if (index >= page_count_) return false;
    selected_page_ = index;
    return true;
}

bool PrintPreviewModel::MoveBy(std::ptrdiff_t delta) noexcept {
    if (page_count_ == 0) return false;
    const auto current = static_cast<std::ptrdiff_t>(selected_page_);
    const auto maximum = static_cast<std::ptrdiff_t>(page_count_ - 1);
    std::ptrdiff_t next = 0;
    if (delta > 0 && delta > maximum - current) next = maximum;
    else if (delta < 0 && delta < -current) next = 0;
    else next = current + delta;
    if (next == current) return false;
    selected_page_ = static_cast<std::size_t>(next);
    return true;
}

bool PrintPreviewModel::SetZoom(double zoom) noexcept {
    if (!std::isfinite(zoom) || zoom < 0.05 || zoom > 16.0) return false;
    zoom_ = zoom;
    scale_mode_ = PrintPreviewScaleMode::custom;
    return true;
}

bool PrintTransactionModel::BeginDocument() noexcept {
    if (state_ != PrintTransactionState::idle) return false;
    state_ = PrintTransactionState::document;
    return true;
}

bool PrintTransactionModel::BeginPage() noexcept {
    if (state_ != PrintTransactionState::document) return false;
    state_ = PrintTransactionState::page;
    return true;
}

bool PrintTransactionModel::EndPage() noexcept {
    if (state_ != PrintTransactionState::page) return false;
    ++completed_pages_;
    state_ = PrintTransactionState::document;
    return true;
}

bool PrintTransactionModel::Complete() noexcept {
    if (state_ != PrintTransactionState::document) return false;
    state_ = PrintTransactionState::completed;
    return true;
}

bool PrintTransactionModel::Abort() noexcept {
    if (state_ != PrintTransactionState::document && state_ != PrintTransactionState::page)
        return false;
    state_ = PrintTransactionState::aborted;
    return true;
}

void PrintTransactionModel::Fail() noexcept {
    if (state_ == PrintTransactionState::idle || state_ == PrintTransactionState::document ||
        state_ == PrintTransactionState::page)
        state_ = PrintTransactionState::failed;
}

DefaultPrinterResult QueryDefaultPrinter() {
    DefaultPrinterResult result;
    DWORD length = 0;
    ::GetDefaultPrinterW(nullptr, &length);
    DWORD error = ::GetLastError();
    if (length == 0) {
        result.error = error;
        result.status = MapPrinterError(error);
        return result;
    }
    std::wstring buffer(length, L'\0');
    if (!::GetDefaultPrinterW(buffer.data(), &length)) {
        result.error = ::GetLastError();
        result.status = MapPrinterError(result.error);
        return result;
    }
    buffer.resize(length > 0 ? length - 1 : 0);
    if (buffer.empty()) {
        result.error = ERROR_INVALID_DATA;
        result.status = PrinterQueryStatus::invalid_data;
        return result;
    }
    result.name = std::move(buffer);
    result.error = ERROR_SUCCESS;
    result.status = PrinterQueryStatus::success;
    return result;
}

PrinterListResult EnumerateLocalPrinters() {
    PrinterListResult result;
    DWORD bytes = 0;
    DWORD count = 0;
    constexpr DWORD flags = PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS;
    ::EnumPrintersW(flags, nullptr, 2, nullptr, 0, &bytes, &count);
    const DWORD first_error = ::GetLastError();
    if (bytes == 0) {
        if (first_error != ERROR_INSUFFICIENT_BUFFER && first_error != ERROR_SUCCESS) {
            result.error = first_error;
            result.status = MapPrinterError(first_error);
            return result;
        }
        result.error = ERROR_SUCCESS;
        result.status = PrinterQueryStatus::success;
        return result;
    }
    std::vector<std::byte> buffer(bytes);
    if (!::EnumPrintersW(flags, nullptr, 2, reinterpret_cast<BYTE*>(buffer.data()), bytes,
                         &bytes, &count)) {
        result.error = ::GetLastError();
        result.status = MapPrinterError(result.error);
        return result;
    }
    const auto default_printer = QueryDefaultPrinter();
    const auto* entries = reinterpret_cast<const PRINTER_INFO_2W*>(buffer.data());
    result.printers.reserve(count);
    for (DWORD index = 0; index < count; ++index) {
        PrinterInfo printer;
        printer.name = Copy(entries[index].pPrinterName);
        if (printer.name.empty()) continue;
        printer.server_name = Copy(entries[index].pServerName);
        printer.driver_name = Copy(entries[index].pDriverName);
        printer.port_name = Copy(entries[index].pPortName);
        printer.attributes = entries[index].Attributes;
        printer.status = entries[index].Status;
        printer.is_default = default_printer && printer.name == default_printer.name;
        result.printers.push_back(std::move(printer));
    }
    result.error = ERROR_SUCCESS;
    result.status = PrinterQueryStatus::success;
    return result;
}

}  // namespace mwtl
