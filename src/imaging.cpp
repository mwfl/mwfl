#include <mwfl/imaging.h>

#include <wincodec.h>
#include <wrl/client.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace mwfl {
namespace {

ImageDecodeStatus MapDecodeError(HRESULT error) noexcept {
    if (error == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) ||
        error == HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND))
        return ImageDecodeStatus::not_found;
    if (error == E_ACCESSDENIED || error == HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED))
        return ImageDecodeStatus::access_denied;
    if (error == WINCODEC_ERR_COMPONENTNOTFOUND || error == WINCODEC_ERR_UNKNOWNIMAGEFORMAT)
        return ImageDecodeStatus::unsupported;
    if (error == WINCODEC_ERR_BADIMAGE || error == WINCODEC_ERR_BADHEADER ||
        error == WINCODEC_ERR_FRAMEMISSING)
        return ImageDecodeStatus::invalid_data;
    if (error == CO_E_NOTINITIALIZED) return ImageDecodeStatus::com_unavailable;
    if (error == E_OUTOFMEMORY) return ImageDecodeStatus::out_of_memory;
    return ImageDecodeStatus::failed;
}

}  // namespace

ImageDecodeResult DecodeImageFile(const std::filesystem::path& path,
                                  ImageDecodeOptions options) {
    ImageDecodeResult result;
    if (path.empty() || options.maximum_dimension == 0 || options.maximum_pixels == 0) {
        result.status = ImageDecodeStatus::invalid_data;
        result.error = E_INVALIDARG;
        return result;
    }
    Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
    result.error = ::CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                      IID_PPV_ARGS(&factory));
    if (FAILED(result.error)) {
        result.status = MapDecodeError(result.error);
        return result;
    }
    Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
    result.error = factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
                                                      WICDecodeMetadataCacheOnDemand, &decoder);
    if (FAILED(result.error)) {
        result.status = MapDecodeError(result.error);
        return result;
    }
    Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
    result.error = decoder->GetFrame(0, &frame);
    if (FAILED(result.error)) {
        result.status = MapDecodeError(result.error);
        return result;
    }
    result.error = frame->GetSize(&result.image.width, &result.image.height);
    if (FAILED(result.error) || result.image.width == 0 || result.image.height == 0) {
        result.status = ImageDecodeStatus::invalid_data;
        return result;
    }
    const std::uint64_t pixels = static_cast<std::uint64_t>(result.image.width) *
                                 result.image.height;
    if (result.image.width > options.maximum_dimension ||
        result.image.height > options.maximum_dimension || pixels > options.maximum_pixels ||
        result.image.width > (std::numeric_limits<std::uint32_t>::max)() / 4) {
        result.status = ImageDecodeStatus::too_large;
        result.error = HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE);
        return result;
    }
    frame->GetResolution(&result.image.dpi_x, &result.image.dpi_y);
    if (!(result.image.dpi_x > 0)) result.image.dpi_x = 96;
    if (!(result.image.dpi_y > 0)) result.image.dpi_y = 96;

    Microsoft::WRL::ComPtr<IWICBitmapSource> source;
    UINT context_count = 0;
    if (SUCCEEDED(frame->GetColorContexts(0, nullptr, &context_count)) && context_count > 0) {
        result.image.has_embedded_color_profile = true;
    }
    if (options.color_manage_to_srgb && context_count > 0) {
        Microsoft::WRL::ComPtr<IWICColorContext> source_context;
        Microsoft::WRL::ComPtr<IWICColorContext> destination_context;
        Microsoft::WRL::ComPtr<IWICColorTransform> transform;
        if (SUCCEEDED(factory->CreateColorContext(&source_context)) &&
            SUCCEEDED(frame->GetColorContexts(1, source_context.GetAddressOf(), &context_count)) &&
            SUCCEEDED(factory->CreateColorContext(&destination_context)) &&
            SUCCEEDED(destination_context->InitializeFromExifColorSpace(1)) &&
            SUCCEEDED(factory->CreateColorTransformer(&transform)) &&
            SUCCEEDED(transform->Initialize(frame.Get(), source_context.Get(),
                                            destination_context.Get(),
                                            GUID_WICPixelFormat32bppPBGRA))) {
            source = transform;
            result.image.color_managed_to_srgb = true;
        }
    }
    if (!source) {
        Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
        result.error = factory->CreateFormatConverter(&converter);
        if (SUCCEEDED(result.error)) {
            result.error = converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppPBGRA,
                                                  WICBitmapDitherTypeNone, nullptr, 0,
                                                  WICBitmapPaletteTypeCustom);
        }
        if (FAILED(result.error)) {
            result.status = MapDecodeError(result.error);
            return result;
        }
        source = converter;
    }

    result.image.stride = result.image.width * 4;
    const std::uint64_t byte_count = static_cast<std::uint64_t>(result.image.stride) *
                                     result.image.height;
    if (byte_count > (std::numeric_limits<UINT>::max)() ||
        byte_count > (std::numeric_limits<std::size_t>::max)()) {
        result.status = ImageDecodeStatus::too_large;
        result.error = HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE);
        return result;
    }
    try {
        result.image.pixels.resize(static_cast<std::size_t>(byte_count));
    } catch (...) {
        result.status = ImageDecodeStatus::out_of_memory;
        result.error = E_OUTOFMEMORY;
        return result;
    }
    result.error = source->CopyPixels(nullptr, result.image.stride,
                                      static_cast<UINT>(byte_count),
                                      reinterpret_cast<BYTE*>(result.image.pixels.data()));
    if (FAILED(result.error)) {
        result.image.pixels.clear();
        result.status = MapDecodeError(result.error);
        return result;
    }
    result.status = ImageDecodeStatus::success;
    result.error = S_OK;
    return result;
}

void ImageViewportModel::SetImageSize(std::uint32_t width, std::uint32_t height) noexcept {
    image_width_ = width;
    image_height_ = height;
    zoom_ = 1;
    pan_ = {};
    ClampPan();
}

void ImageViewportModel::SetViewport(SizeDip viewport) noexcept {
    viewport_.width.value = (std::max)(viewport.width.value, 0.0f);
    viewport_.height.value = (std::max)(viewport.height.value, 0.0f);
    ClampPan();
}

bool ImageViewportModel::Fit(bool allow_upscale) noexcept {
    if (image_width_ == 0 || image_height_ == 0 || viewport_.width.value <= 0 ||
        viewport_.height.value <= 0)
        return false;
    zoom_ = (std::min)(viewport_.width.value / static_cast<float>(image_width_),
                       viewport_.height.value / static_cast<float>(image_height_));
    if (!allow_upscale) zoom_ = (std::min)(zoom_, 1.0f);
    zoom_ = std::clamp(zoom_, 1.0f / 64.0f, 64.0f);
    pan_ = {};
    return true;
}

bool ImageViewportModel::SetZoom(float zoom, PointDip anchor) noexcept {
    if (image_width_ == 0 || image_height_ == 0 || !std::isfinite(zoom) || zoom <= 0)
        return false;
    const ImageViewportTransform before = GetTransform();
    const float image_x = (anchor.x.value - before.origin.x.value) / before.scale;
    const float image_y = (anchor.y.value - before.origin.y.value) / before.scale;
    zoom_ = std::clamp(zoom, 1.0f / 64.0f, 64.0f);
    const float centered_x = (viewport_.width.value - image_width_ * zoom_) * 0.5f;
    const float centered_y = (viewport_.height.value - image_height_ * zoom_) * 0.5f;
    pan_.x.value = anchor.x.value - image_x * zoom_ - centered_x;
    pan_.y.value = anchor.y.value - image_y * zoom_ - centered_y;
    ClampPan();
    return true;
}

bool ImageViewportModel::ZoomBy(float factor, PointDip anchor) noexcept {
    return std::isfinite(factor) && factor > 0 && SetZoom(zoom_ * factor, anchor);
}

bool ImageViewportModel::PanBy(SizeDip delta) noexcept {
    if (!std::isfinite(delta.width.value) || !std::isfinite(delta.height.value)) return false;
    pan_.x.value += delta.width.value;
    pan_.y.value += delta.height.value;
    ClampPan();
    return true;
}

ImageViewportTransform ImageViewportModel::GetTransform() const noexcept {
    const SizeDip rendered{Dip(static_cast<float>(image_width_) * zoom_),
                           Dip(static_cast<float>(image_height_) * zoom_)};
    return {zoom_,
            {Dip((viewport_.width.value - rendered.width.value) * 0.5f + pan_.x.value),
             Dip((viewport_.height.value - rendered.height.value) * 0.5f + pan_.y.value)},
            rendered};
}

void ImageViewportModel::ClampPan() noexcept {
    const float overflow_x = (std::max)(0.0f, image_width_ * zoom_ - viewport_.width.value);
    const float overflow_y = (std::max)(0.0f, image_height_ * zoom_ - viewport_.height.value);
    pan_.x.value = std::clamp(pan_.x.value, -overflow_x * 0.5f, overflow_x * 0.5f);
    pan_.y.value = std::clamp(pan_.y.value, -overflow_y * 0.5f, overflow_y * 0.5f);
}

}  // namespace mwfl
