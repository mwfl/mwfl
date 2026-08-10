#include <mwtl/graphics.h>

#include <filesystem>

mwtl::GraphicsResult ExportDiagram(const std::filesystem::path& path) noexcept {
    return mwtl::ExportGdiPlusPng(path, 640, 480,
        [](Gdiplus::Graphics& graphics) {
            graphics.Clear(Gdiplus::Color(255, 248, 250, 252));
            Gdiplus::Pen pen(Gdiplus::Color(255, 20, 90, 180), 3.0f);
            graphics.DrawRectangle(&pen, 20, 20, 600, 440);
        });
}

mwtl::EnhancedMetafileResult RecordDiagram() noexcept {
    return mwtl::RecordEnhancedMetafile(nullptr, L"diagram", [](HDC dc) {
        ::Rectangle(dc, 0, 0, 640, 480);
    });
}
