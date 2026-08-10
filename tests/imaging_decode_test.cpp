#include <mwfl/imaging.h>

#include <wil/resource.h>

#include <array>
#include <filesystem>

namespace {
bool WriteBmp(const std::filesystem::path& path) {
    BITMAPFILEHEADER file{};
    BITMAPINFOHEADER info{};
    constexpr std::array<BYTE, 16> pixels{
        0, 0, 255, 0, 255, 0, 0, 0,
        255, 0, 0, 255, 255, 255, 0, 0};
    file.bfType = 0x4D42;
    file.bfOffBits = sizeof(file) + sizeof(info);
    file.bfSize = file.bfOffBits + static_cast<DWORD>(pixels.size());
    info.biSize = sizeof(info);
    info.biWidth = 2;
    info.biHeight = 2;
    info.biPlanes = 1;
    info.biBitCount = 24;
    info.biCompression = BI_RGB;
    info.biSizeImage = static_cast<DWORD>(pixels.size());
    wil::unique_hfile handle(::CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                           FILE_ATTRIBUTE_TEMPORARY, nullptr));
    if (!handle) return false;
    DWORD written = 0;
    return ::WriteFile(handle.get(), &file, sizeof(file), &written, nullptr) &&
           written == sizeof(file) &&
           ::WriteFile(handle.get(), &info, sizeof(info), &written, nullptr) &&
           written == sizeof(info) &&
           ::WriteFile(handle.get(), pixels.data(), static_cast<DWORD>(pixels.size()), &written,
                       nullptr) &&
           written == pixels.size();
}
}

int main() {
    using namespace mwfl;
    const auto before_com = DecodeImageFile(L"missing.bmp");
    if (before_com.status != ImageDecodeStatus::com_unavailable) return 1;
    if (FAILED(::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) return 2;
    const auto uninitialize = wil::scope_exit([] { ::CoUninitialize(); });

    const auto missing = DecodeImageFile(L"definitely-missing-mwfl-image.bmp");
    if (missing.status != ImageDecodeStatus::not_found) return 3;
    const auto path = std::filesystem::temp_directory_path() /
                      (L"mwfl-image-" + std::to_wstring(::GetCurrentProcessId()) + L".bmp");
    if (!WriteBmp(path)) return 4;
    const auto cleanup = wil::scope_exit([&] { ::DeleteFileW(path.c_str()); });

    const auto too_large = DecodeImageFile(path, {.maximum_dimension = 1});
    if (too_large.status != ImageDecodeStatus::too_large || too_large.image) return 5;
    const auto decoded = DecodeImageFile(path);
    if (!decoded || !decoded.image || decoded.image.width != 2 || decoded.image.height != 2 ||
        decoded.image.stride != 8 || decoded.image.pixels.size() != 16 ||
        decoded.image.has_embedded_color_profile || decoded.image.color_managed_to_srgb)
        return 6;
    return 0;
}
