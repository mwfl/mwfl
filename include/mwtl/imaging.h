#pragma once

#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

#include <mwtl/dpi.h>

namespace mwtl {

enum class ImageDecodeStatus {
    success,
    not_found,
    access_denied,
    unsupported,
    invalid_data,
    too_large,
    com_unavailable,
    out_of_memory,
    failed,
};

struct ImageDecodeOptions {
    std::uint32_t maximum_dimension = 32768;
    std::uint64_t maximum_pixels = 67'108'864;  // 256 MiB at normalized BGRA8.
    bool color_manage_to_srgb = true;
};

// Application-owned normalized pixels. Rows are top-down, format is
// premultiplied BGRA8, and stride is width * 4. Embedded-profile presence and
// whether WIC successfully transformed it to sRGB are explicit metadata.
struct DecodedImage {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t stride = 0;
    double dpi_x = 96;
    double dpi_y = 96;
    bool has_embedded_color_profile = false;
    bool color_managed_to_srgb = false;
    std::vector<std::byte> pixels;

    constexpr explicit operator bool() const noexcept {
        return width > 0 && height > 0 && stride == width * 4 &&
               pixels.size() == static_cast<std::size_t>(stride) * height;
    }
};

struct ImageDecodeResult {
    DecodedImage image;
    ImageDecodeStatus status = ImageDecodeStatus::failed;
    HRESULT error = E_FAIL;
    explicit operator bool() const noexcept { return status == ImageDecodeStatus::success; }
};

// Requires COM initialized on the calling thread. WIC metadata and decoding are
// synchronous; no HWND or GPU is required. Size limits are checked before pixel
// allocation. Unknown/invalid embedded profiles fall back to normalized BGRA8
// without claiming color management.
ImageDecodeResult DecodeImageFile(const std::filesystem::path& path,
                                  ImageDecodeOptions options = {});

struct ImageViewportTransform {
    float scale = 1;
    PointDip origin{};
    SizeDip rendered_size{};
};

// Pure zoom/pan/fit model. Image units are pixels, viewport/pan/origin are DIPs.
// Zoom is clamped to [1/64, 64]. Fit never upscales unless requested.
class ImageViewportModel final {
public:
    void SetImageSize(std::uint32_t width, std::uint32_t height) noexcept;
    void SetViewport(SizeDip viewport) noexcept;
    bool Fit(bool allow_upscale = false) noexcept;
    bool SetZoom(float zoom, PointDip anchor) noexcept;
    bool ZoomBy(float factor, PointDip anchor) noexcept;
    bool PanBy(SizeDip delta) noexcept;

    std::uint32_t GetImageWidth() const noexcept { return image_width_; }
    std::uint32_t GetImageHeight() const noexcept { return image_height_; }
    float GetZoom() const noexcept { return zoom_; }
    PointDip GetPan() const noexcept { return pan_; }
    SizeDip GetViewport() const noexcept { return viewport_; }
    ImageViewportTransform GetTransform() const noexcept;

private:
    void ClampPan() noexcept;
    std::uint32_t image_width_ = 0;
    std::uint32_t image_height_ = 0;
    SizeDip viewport_{};
    float zoom_ = 1;
    PointDip pan_{};
};

}  // namespace mwtl
