#pragma once

#include <windows.h>
#include <gdiplus.h>

#include <cstdint>
#include <exception>
#include <filesystem>
#include <functional>
#include <string_view>

namespace mwtl {

enum class GraphicsStatus {
    success,
    invalid_argument,
    not_found,
    access_denied,
    too_large,
    native_failure,
    callback_failed,
    codec_unavailable,
    wrong_thread,
    invalid_state,
};

struct GraphicsResult {
    GraphicsStatus status = GraphicsStatus::native_failure;
    DWORD error = ERROR_GEN_FAILURE;
    std::exception_ptr exception{};
    explicit operator bool() const noexcept { return status == GraphicsStatus::success; }
};

// Move-only ownership of one HENHMETAFILE. Get() is borrowed; Release()
// transfers ownership to the caller, who must call DeleteEnhMetaFile.
class EnhancedMetafile final {
public:
    EnhancedMetafile() noexcept = default;
    explicit EnhancedMetafile(HENHMETAFILE value) noexcept : value_(value) {}
    ~EnhancedMetafile() noexcept;
    EnhancedMetafile(const EnhancedMetafile&) = delete;
    EnhancedMetafile& operator=(const EnhancedMetafile&) = delete;
    EnhancedMetafile(EnhancedMetafile&& other) noexcept;
    EnhancedMetafile& operator=(EnhancedMetafile&& other) noexcept;

    HENHMETAFILE Get() const noexcept { return value_; }
    explicit operator bool() const noexcept { return value_ != nullptr; }
    HENHMETAFILE Release() noexcept;
    void Reset(HENHMETAFILE value = nullptr) noexcept;

private:
    HENHMETAFILE value_ = nullptr;
};

struct EnhancedMetafileResult : GraphicsResult {
    EnhancedMetafile metafile;
};

using EnhancedMetafileRecorder = std::function<void(HDC)>;

// The optional frame is in 0.01 millimeter units, as required by
// CreateEnhMetaFile. The callback borrows its HDC only for the call. On callback
// failure the recording HDC is closed and the partial metafile is deleted.
EnhancedMetafileResult RecordEnhancedMetafile(
    const RECT* frame_hundredth_mm, std::wstring_view description,
    const EnhancedMetafileRecorder& recorder, HDC reference = nullptr) noexcept;
EnhancedMetafileResult LoadEnhancedMetafile(const std::filesystem::path& path) noexcept;
GraphicsResult SaveEnhancedMetafile(const EnhancedMetafile& metafile,
                                    const std::filesystem::path& path) noexcept;
GraphicsResult PlayEnhancedMetafile(HDC destination,
                                    const EnhancedMetafile& metafile,
                                    RECT destination_pixels) noexcept;

// Explicit process GDI+ token. Startup and shutdown are paired by this object;
// destruction must occur on its creating thread. GetToken is diagnostic only.
class GdiPlusSession final {
public:
    GdiPlusSession() noexcept = default;
    ~GdiPlusSession() noexcept;
    GdiPlusSession(const GdiPlusSession&) = delete;
    GdiPlusSession& operator=(const GdiPlusSession&) = delete;
    GdiPlusSession(GdiPlusSession&& other) noexcept;
    GdiPlusSession& operator=(GdiPlusSession&& other) noexcept;

    GraphicsResult Start() noexcept;
    GraphicsResult Stop() noexcept;
    bool IsStarted() const noexcept { return token_ != 0; }
    ULONG_PTR GetToken() const noexcept { return token_; }

private:
    ULONG_PTR token_ = 0;
    DWORD thread_id_ = 0;
};

using GdiPlusDrawHandler = std::function<void(Gdiplus::Graphics&)>;

// Creates a bounded 32bpp ARGB bitmap, invokes the callback without retaining
// Graphics, and encodes PNG atomically through a sibling temporary file.
GraphicsResult ExportGdiPlusPng(const std::filesystem::path& path,
                                std::uint32_t width, std::uint32_t height,
                                const GdiPlusDrawHandler& draw) noexcept;

}  // namespace mwtl
