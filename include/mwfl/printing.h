#pragma once

#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace mwfl {

struct PrintExtentMicrons {
    std::int64_t width = 0;
    std::int64_t height = 0;
    constexpr bool IsValid() const noexcept { return width > 0 && height > 0; }
};

struct PrintMarginsMicrons {
    std::int64_t left = 0;
    std::int64_t top = 0;
    std::int64_t right = 0;
    std::int64_t bottom = 0;
    constexpr bool IsValid() const noexcept {
        return left >= 0 && top >= 0 && right >= 0 && bottom >= 0;
    }
};

struct PrintLayout {
    PrintExtentMicrons paper{};
    PrintMarginsMicrons margins{};
    constexpr bool IsValid() const noexcept {
        return paper.IsValid() && margins.IsValid() &&
               margins.left + margins.right < paper.width &&
               margins.top + margins.bottom < paper.height;
    }
    constexpr PrintExtentMicrons ContentExtent() const noexcept {
        return IsValid() ? PrintExtentMicrons{paper.width - margins.left - margins.right,
                                               paper.height - margins.top - margins.bottom}
                         : PrintExtentMicrons{};
    }
};

struct PrintPageId {
    std::uint64_t value = 0;
    friend constexpr bool operator==(PrintPageId, PrintPageId) noexcept = default;
};

struct PrintPage {
    PrintPageId id{};
    std::size_t index = 0;
    std::int64_t content_begin = 0;
    std::int64_t content_end = 0;
};

// Pure deterministic pagination for one-dimensional application-owned content.
// All physical dimensions are micrometres. Content offsets use caller-defined
// units and remain stable while content_length and units_per_page are unchanged.
std::vector<PrintPage> PaginateContent(std::int64_t content_length,
                                       std::int64_t units_per_page);

struct PrintPageRange {
    // Inclusive, zero-based indices. An empty document has no valid range.
    std::size_t first = 0;
    std::size_t last = 0;
    bool IsValidFor(std::size_t page_count) const noexcept;
};

enum class PrintPreviewScaleMode { custom, fit_page, fit_width };

class PrintPreviewModel final {
public:
    void SetPageCount(std::size_t count) noexcept;
    bool SelectPage(std::size_t index) noexcept;
    bool MoveBy(std::ptrdiff_t delta) noexcept;
    bool SetZoom(double zoom) noexcept;
    void SetScaleMode(PrintPreviewScaleMode mode) noexcept { scale_mode_ = mode; }

    std::size_t GetPageCount() const noexcept { return page_count_; }
    std::size_t GetSelectedPage() const noexcept { return selected_page_; }
    double GetZoom() const noexcept { return zoom_; }
    PrintPreviewScaleMode GetScaleMode() const noexcept { return scale_mode_; }

private:
    std::size_t page_count_ = 0;
    std::size_t selected_page_ = 0;
    double zoom_ = 1.0;
    PrintPreviewScaleMode scale_mode_ = PrintPreviewScaleMode::fit_page;
};

enum class PrintTransactionState { idle, document, page, completed, aborted, failed };

// Pure state machine shared by native print jobs and deterministic tests.
class PrintTransactionModel final {
public:
    bool BeginDocument() noexcept;
    bool BeginPage() noexcept;
    bool EndPage() noexcept;
    bool Complete() noexcept;
    bool Abort() noexcept;
    void Fail() noexcept;
    PrintTransactionState GetState() const noexcept { return state_; }
    std::size_t GetCompletedPageCount() const noexcept { return completed_pages_; }

private:
    PrintTransactionState state_ = PrintTransactionState::idle;
    std::size_t completed_pages_ = 0;
};

enum class PrinterQueryStatus { success, no_default_printer, unavailable, invalid_data, failed };

struct PrinterInfo {
    std::wstring name;
    std::wstring server_name;
    std::wstring driver_name;
    std::wstring port_name;
    DWORD attributes = 0;
    DWORD status = 0;
    bool is_default = false;
};

struct PrinterListResult {
    std::vector<PrinterInfo> printers;
    PrinterQueryStatus status = PrinterQueryStatus::failed;
    DWORD error = ERROR_GEN_FAILURE;
    explicit operator bool() const noexcept { return status == PrinterQueryStatus::success; }
};

struct DefaultPrinterResult {
    std::wstring name;
    PrinterQueryStatus status = PrinterQueryStatus::failed;
    DWORD error = ERROR_GEN_FAILURE;
    explicit operator bool() const noexcept { return status == PrinterQueryStatus::success; }
};

// Read-only, synchronous queries. They do not show UI or retain printer handles.
DefaultPrinterResult QueryDefaultPrinter();
PrinterListResult EnumerateLocalPrinters();

}  // namespace mwfl
