#pragma once

#include <windows.h>
#include <objidl.h>
#include <wrl/client.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace mwtl {

enum class OleDataStatus {
    success,
    format_unavailable,
    invalid_data,
    too_large,
    allocation_failure,
    com_failure,
    renderer_failure,
};

struct OleBytesResult {
    std::vector<std::byte> bytes;
    OleDataStatus status = OleDataStatus::com_failure;
    HRESULT error = E_FAIL;
    explicit operator bool() const noexcept { return status == OleDataStatus::success; }
};

struct OleTextResult {
    std::wstring text;
    OleDataStatus status = OleDataStatus::com_failure;
    HRESULT error = E_FAIL;
    explicit operator bool() const noexcept { return status == OleDataStatus::success; }
};

struct OleFilesResult {
    std::vector<std::filesystem::path> files;
    OleDataStatus status = OleDataStatus::com_failure;
    HRESULT error = E_FAIL;
    explicit operator bool() const noexcept { return status == OleDataStatus::success; }
};

using OleDelayedBytesRenderer = std::function<std::vector<std::byte>()>;

// Builds an immutable IDataObject. Payload bytes are copied; no application
// pointers cross the COM boundary. Delayed renderers execute synchronously on
// the calling apartment and exceptions are contained as COM failures.
class OleDataObjectBuilder final {
public:
    explicit OleDataObjectBuilder(std::size_t maximum_payload_bytes = 16 * 1024 * 1024);

    bool AddUnicodeText(std::wstring_view text);
    bool AddFiles(std::span<const std::filesystem::path> files);
    bool AddBytes(CLIPFORMAT format, std::span<const std::byte> bytes);
    bool AddDelayedBytes(CLIPFORMAT format, OleDelayedBytesRenderer renderer);
    Microsoft::WRL::ComPtr<IDataObject> Build() const;
    std::size_t GetFormatCount() const noexcept;

private:
    struct Entry {
        CLIPFORMAT format = 0;
        std::vector<std::byte> bytes;
        OleDelayedBytesRenderer renderer;
    };
    bool CanAdd(CLIPFORMAT format) const noexcept;
    std::size_t maximum_payload_bytes_;
    std::vector<Entry> entries_;
};

CLIPFORMAT RegisterOleFormat(std::wstring_view name) noexcept;
OleBytesResult ReadOleBytes(IDataObject& object, CLIPFORMAT format,
                            std::size_t maximum_bytes = 16 * 1024 * 1024);
OleTextResult ReadOleUnicodeText(IDataObject& object,
                                 std::size_t maximum_characters = 4 * 1024 * 1024);
OleFilesResult ReadOleFiles(IDataObject& object, std::size_t maximum_files = 4096,
                            std::size_t maximum_path_characters = 32768);

// Pure OLE drop-effect negotiation. preferred must also be in allowed.
DWORD NegotiateDropEffect(DWORD allowed, DWORD key_state,
                          DWORD preferred = DROPEFFECT_COPY) noexcept;

}  // namespace mwtl
