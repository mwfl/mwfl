#include <mwtl/graphics.h>

#include <algorithm>
#include <array>
#include <limits>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace mwtl {
namespace {

GraphicsResult Result(GraphicsStatus status, DWORD error = ERROR_SUCCESS) noexcept {
    return {status, error};
}

GraphicsStatus MapFileError(DWORD error) noexcept {
    if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND)
        return GraphicsStatus::not_found;
    if (error == ERROR_ACCESS_DENIED || error == ERROR_SHARING_VIOLATION)
        return GraphicsStatus::access_denied;
    return GraphicsStatus::native_failure;
}

bool ValidRect(RECT value) noexcept {
    return value.right > value.left && value.bottom > value.top;
}

bool FindPngEncoder(CLSID& result) noexcept {
    UINT count = 0;
    UINT bytes = 0;
    if (Gdiplus::GetImageEncodersSize(&count, &bytes) != Gdiplus::Ok ||
        count == 0 || bytes == 0 || bytes > 16 * 1024 * 1024) return false;
    try {
        std::vector<std::byte> storage(bytes);
        auto* encoders = reinterpret_cast<Gdiplus::ImageCodecInfo*>(storage.data());
        if (Gdiplus::GetImageEncoders(count, bytes, encoders) != Gdiplus::Ok) return false;
        for (UINT index = 0; index < count; ++index) {
            if (encoders[index].MimeType &&
                ::CompareStringOrdinal(encoders[index].MimeType, -1, L"image/png", -1,
                                       TRUE) == CSTR_EQUAL) {
                result = encoders[index].Clsid;
                return true;
            }
        }
    } catch (...) { return false; }
    return false;
}

}  // namespace

EnhancedMetafile::~EnhancedMetafile() noexcept { Reset(); }

EnhancedMetafile::EnhancedMetafile(EnhancedMetafile&& other) noexcept
    : value_(other.Release()) {}

EnhancedMetafile& EnhancedMetafile::operator=(EnhancedMetafile&& other) noexcept {
    if (this != &other) Reset(other.Release());
    return *this;
}

HENHMETAFILE EnhancedMetafile::Release() noexcept {
    return std::exchange(value_, nullptr);
}

void EnhancedMetafile::Reset(HENHMETAFILE value) noexcept {
    if (value_) ::DeleteEnhMetaFile(value_);
    value_ = value;
}

EnhancedMetafileResult RecordEnhancedMetafile(
    const RECT* frame, std::wstring_view description,
    const EnhancedMetafileRecorder& recorder, HDC reference) noexcept {
    EnhancedMetafileResult result;
    if (!recorder || (frame && !ValidRect(*frame)) || description.size() > 32767) {
        result.status = GraphicsStatus::invalid_argument;
        result.error = ERROR_INVALID_PARAMETER;
        return result;
    }
    std::wstring terminated;
    const wchar_t* text = nullptr;
    try {
        if (!description.empty()) {
            terminated.assign(description);
            terminated.push_back(L'\0');
            terminated.push_back(L'\0');
            text = terminated.c_str();
        }
    } catch (...) {
        result.status = GraphicsStatus::too_large;
        result.error = ERROR_OUTOFMEMORY;
        result.exception = std::current_exception();
        return result;
    }
    HDC recording = ::CreateEnhMetaFileW(reference, nullptr, frame, text);
    if (!recording) {
        result.status = GraphicsStatus::native_failure;
        result.error = ::GetLastError();
        return result;
    }
    try { recorder(recording); }
    catch (...) { result.exception = std::current_exception(); }
    HENHMETAFILE handle = ::CloseEnhMetaFile(recording);
    if (!handle) {
        result.status = GraphicsStatus::native_failure;
        result.error = ::GetLastError();
        return result;
    }
    if (result.exception) {
        ::DeleteEnhMetaFile(handle);
        result.status = GraphicsStatus::callback_failed;
        result.error = ERROR_UNHANDLED_EXCEPTION;
        return result;
    }
    result.metafile.Reset(handle);
    result.status = GraphicsStatus::success;
    result.error = ERROR_SUCCESS;
    return result;
}

EnhancedMetafileResult LoadEnhancedMetafile(const std::filesystem::path& path) noexcept {
    EnhancedMetafileResult result;
    if (path.empty()) {
        result.status = GraphicsStatus::invalid_argument;
        result.error = ERROR_INVALID_PARAMETER;
        return result;
    }
    HENHMETAFILE handle = ::GetEnhMetaFileW(path.c_str());
    if (!handle) {
        result.error = ::GetLastError();
        result.status = MapFileError(result.error);
        return result;
    }
    result.metafile.Reset(handle);
    result.status = GraphicsStatus::success;
    result.error = ERROR_SUCCESS;
    return result;
}

GraphicsResult SaveEnhancedMetafile(const EnhancedMetafile& metafile,
                                    const std::filesystem::path& path) noexcept {
    if (!metafile || path.empty())
        return Result(GraphicsStatus::invalid_argument, ERROR_INVALID_PARAMETER);
    HENHMETAFILE copy = ::CopyEnhMetaFileW(metafile.Get(), path.c_str());
    if (!copy) {
        const DWORD error = ::GetLastError();
        return Result(MapFileError(error), error);
    }
    ::DeleteEnhMetaFile(copy);
    return Result(GraphicsStatus::success);
}

GraphicsResult PlayEnhancedMetafile(HDC destination,
                                    const EnhancedMetafile& metafile,
                                    RECT bounds) noexcept {
    if (!destination || !metafile || !ValidRect(bounds))
        return Result(GraphicsStatus::invalid_argument, ERROR_INVALID_PARAMETER);
    if (!::PlayEnhMetaFile(destination, metafile.Get(), &bounds))
        return Result(GraphicsStatus::native_failure, ::GetLastError());
    return Result(GraphicsStatus::success);
}

GdiPlusSession::~GdiPlusSession() noexcept { static_cast<void>(Stop()); }

GdiPlusSession::GdiPlusSession(GdiPlusSession&& other) noexcept
    : token_(std::exchange(other.token_, 0)),
      thread_id_(std::exchange(other.thread_id_, 0)) {}

GdiPlusSession& GdiPlusSession::operator=(GdiPlusSession&& other) noexcept {
    if (this != &other) {
        static_cast<void>(Stop());
        token_ = std::exchange(other.token_, 0);
        thread_id_ = std::exchange(other.thread_id_, 0);
    }
    return *this;
}

GraphicsResult GdiPlusSession::Start() noexcept {
    if (token_) return Result(GraphicsStatus::invalid_state, ERROR_ALREADY_INITIALIZED);
    Gdiplus::GdiplusStartupInput input;
    const auto status = Gdiplus::GdiplusStartup(&token_, &input, nullptr);
    if (status != Gdiplus::Ok) {
        token_ = 0;
        return Result(GraphicsStatus::native_failure, static_cast<DWORD>(status));
    }
    thread_id_ = ::GetCurrentThreadId();
    return Result(GraphicsStatus::success);
}

GraphicsResult GdiPlusSession::Stop() noexcept {
    if (!token_) return Result(GraphicsStatus::success);
    if (::GetCurrentThreadId() != thread_id_)
        return Result(GraphicsStatus::wrong_thread, ERROR_INVALID_THREAD_ID);
    Gdiplus::GdiplusShutdown(token_);
    token_ = 0;
    thread_id_ = 0;
    return Result(GraphicsStatus::success);
}

GraphicsResult ExportGdiPlusPng(const std::filesystem::path& path,
                                std::uint32_t width, std::uint32_t height,
                                const GdiPlusDrawHandler& draw) noexcept {
    constexpr std::uint64_t MaximumPixels = 67'108'864;
    if (path.empty() || width == 0 || height == 0 || width > 32768 || height > 32768 ||
        static_cast<std::uint64_t>(width) * height > MaximumPixels || !draw)
        return Result(GraphicsStatus::invalid_argument, ERROR_INVALID_PARAMETER);
    GdiPlusSession session;
    auto started = session.Start();
    if (!started) return started;
    Gdiplus::Bitmap bitmap(static_cast<INT>(width), static_cast<INT>(height),
                           PixelFormat32bppARGB);
    if (bitmap.GetLastStatus() != Gdiplus::Ok)
        return Result(GraphicsStatus::native_failure,
                      static_cast<DWORD>(bitmap.GetLastStatus()));
    Gdiplus::Graphics graphics(&bitmap);
    if (graphics.GetLastStatus() != Gdiplus::Ok)
        return Result(GraphicsStatus::native_failure,
                      static_cast<DWORD>(graphics.GetLastStatus()));
    try { draw(graphics); }
    catch (...) {
        return {GraphicsStatus::callback_failed, ERROR_UNHANDLED_EXCEPTION,
                std::current_exception()};
    }
    CLSID encoder{};
    if (!FindPngEncoder(encoder))
        return Result(GraphicsStatus::codec_unavailable, ERROR_NOT_SUPPORTED);
    std::filesystem::path temporary = path;
    temporary += L".mwtl-" + std::to_wstring(::GetCurrentProcessId()) + L"-" +
                 std::to_wstring(::GetCurrentThreadId()) + L".tmp";
    std::error_code ignored;
    std::filesystem::remove(temporary, ignored);
    const auto saved = bitmap.Save(temporary.c_str(), &encoder, nullptr);
    if (saved != Gdiplus::Ok) {
        std::filesystem::remove(temporary, ignored);
        return Result(GraphicsStatus::native_failure, static_cast<DWORD>(saved));
    }
    if (!::MoveFileExW(temporary.c_str(), path.c_str(),
                       MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        const DWORD error = ::GetLastError();
        std::filesystem::remove(temporary, ignored);
        return Result(MapFileError(error), error);
    }
    return Result(GraphicsStatus::success);
}

}  // namespace mwtl
