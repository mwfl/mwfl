#include <mwtl/graphics.h>

#include <windows.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <stdexcept>

int main() {
    using namespace mwtl;
    RECT invalid{0, 0, 0, 10};
    if (RecordEnhancedMetafile(&invalid, L"invalid", [](HDC) {}).status !=
            GraphicsStatus::invalid_argument ||
        RecordEnhancedMetafile(nullptr, L"", {}).status !=
            GraphicsStatus::invalid_argument)
        return 1;

    auto recorded = RecordEnhancedMetafile(nullptr, L"mwtl\0graphics", [](HDC dc) {
        ::Rectangle(dc, 0, 0, 120, 80);
        ::MoveToEx(dc, 0, 0, nullptr);
        ::LineTo(dc, 120, 80);
    });
    if (!recorded || !recorded.metafile) return 2;
    auto failed = RecordEnhancedMetafile(nullptr, L"failure", [](HDC) {
        throw std::runtime_error("record");
    });
    if (failed.status != GraphicsStatus::callback_failed || !failed.exception ||
        failed.metafile) return 3;

    const auto root = std::filesystem::temp_directory_path();
    const auto suffix = std::to_wstring(::GetCurrentProcessId());
    const auto emf_path = root / (L"mwtl-graphics-" + suffix + L".emf");
    const auto png_path = root / (L"mwtl-graphics-" + suffix + L".png");
    const auto bad_path = root / (L"mwtl-graphics-bad-" + suffix + L".emf");
    std::error_code ignored;
    std::filesystem::remove(emf_path, ignored);
    std::filesystem::remove(png_path, ignored);
    std::filesystem::remove(bad_path, ignored);
    {
        std::ofstream bad(bad_path, std::ios::binary);
        bad << "not an enhanced metafile";
    }
    if (LoadEnhancedMetafile(bad_path).status != GraphicsStatus::native_failure)
        return 17;
    if (!SaveEnhancedMetafile(recorded.metafile, emf_path)) return 4;
    auto loaded = LoadEnhancedMetafile(emf_path);
    if (!loaded || !loaded.metafile) return 5;

    HDC screen = ::GetDC(nullptr);
    HDC memory = ::CreateCompatibleDC(screen);
    HBITMAP bitmap = ::CreateCompatibleBitmap(screen, 160, 120);
    HGDIOBJ previous = ::SelectObject(memory, bitmap);
    ::ReleaseDC(nullptr, screen);
    if (!memory || !bitmap || !PlayEnhancedMetafile(memory, loaded.metafile,
                                                     {0, 0, 160, 120})) return 6;
    if (PlayEnhancedMetafile(nullptr, loaded.metafile, {0, 0, 1, 1}).status !=
        GraphicsStatus::invalid_argument) return 7;
    ::SelectObject(memory, previous);
    ::DeleteObject(bitmap);
    ::DeleteDC(memory);

    EnhancedMetafile moved = std::move(loaded.metafile);
    if (!moved || loaded.metafile) return 8;
    HENHMETAFILE raw = moved.Release();
    if (!raw || moved) return 9;
    ::DeleteEnhMetaFile(raw);

    GdiPlusSession session;
    if (!session.Start() || !session.IsStarted() || session.GetToken() == 0 ||
        session.Start().status != GraphicsStatus::invalid_state || !session.Stop() ||
        session.IsStarted() || !session.Stop()) return 10;
    if (ExportGdiPlusPng(png_path, 0, 64, [](Gdiplus::Graphics&) {}).status !=
            GraphicsStatus::invalid_argument ||
        ExportGdiPlusPng(png_path, 40000, 1, [](Gdiplus::Graphics&) {}).status !=
            GraphicsStatus::invalid_argument) return 11;
    auto callback = ExportGdiPlusPng(png_path, 64, 48, [](Gdiplus::Graphics&) {
        throw std::runtime_error("draw");
    });
    if (callback.status != GraphicsStatus::callback_failed || !callback.exception ||
        std::filesystem::exists(png_path)) return 12;
    if (!ExportGdiPlusPng(png_path, 64, 48, [](Gdiplus::Graphics& graphics) {
            graphics.Clear(Gdiplus::Color(255, 20, 40, 80));
            Gdiplus::Pen pen(Gdiplus::Color(255, 255, 255, 255), 2.0f);
            graphics.DrawRectangle(&pen, 4, 4, 55, 39);
        })) return 13;
    bool transform_observed = false;
    if (!ExportGdiPlusPng(png_path, 32, 32,
            [&](Gdiplus::Graphics& graphics) {
                Gdiplus::Matrix transform;
                if (transform.Translate(3.0f, 5.0f) != Gdiplus::Ok ||
                    transform.Scale(2.0f, 2.0f) != Gdiplus::Ok ||
                    graphics.SetTransform(&transform) != Gdiplus::Ok) return;
                Gdiplus::Matrix observed;
                Gdiplus::REAL elements[6]{};
                if (graphics.GetTransform(&observed) != Gdiplus::Ok ||
                    observed.GetElements(elements) != Gdiplus::Ok) return;
                transform_observed = graphics.GetDpiX() > 0.0f &&
                    graphics.GetDpiY() > 0.0f && elements[0] == 2.0f &&
                    elements[3] == 2.0f && elements[4] == 3.0f &&
                    elements[5] == 5.0f;
                Gdiplus::SolidBrush brush(Gdiplus::Color(255, 12, 34, 56));
                graphics.FillRectangle(&brush, 0, 0, 8, 8);
            }) || !transform_observed) return 19;
    std::ifstream png(png_path, std::ios::binary);
    std::array<unsigned char, 8> signature{};
    png.read(reinterpret_cast<char*>(signature.data()), signature.size());
    constexpr std::array<unsigned char, 8> expected{137, 80, 78, 71, 13, 10, 26, 10};
    if (signature != expected) return 14;
    png.close();
    if (!ExportGdiPlusPng(png_path, 16, 16, [](Gdiplus::Graphics& graphics) {
            graphics.Clear(Gdiplus::Color(255, 1, 2, 3));
        })) return 18;

    const DWORD initial_gdi = ::GetGuiResources(::GetCurrentProcess(), GR_GDIOBJECTS);
    for (int cycle = 0; cycle != 100; ++cycle) {
        auto item = RecordEnhancedMetafile(nullptr, L"stress", [](HDC dc) {
            ::Ellipse(dc, 0, 0, 20, 20);
        });
        if (!item) return 15;
    }
    const DWORD final_gdi = ::GetGuiResources(::GetCurrentProcess(), GR_GDIOBJECTS);
    std::filesystem::remove(emf_path, ignored);
    std::filesystem::remove(png_path, ignored);
    std::filesystem::remove(bad_path, ignored);
    return final_gdi <= initial_gdi + 2 ? 0 : 16;
}
