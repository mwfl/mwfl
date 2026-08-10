#include <mwfl/graphics.h>

#include <filesystem>

mwfl::GraphicsResult ExportDiagram(const std::filesystem::path& path) noexcept {
    return mwfl::ExportGdiPlusPng(path, 640, 480,
        [](Gdiplus::Graphics& graphics) {
            graphics.Clear(Gdiplus::Color(255, 248, 250, 252));
            Gdiplus::Pen pen(Gdiplus::Color(255, 20, 90, 180), 3.0f);
            graphics.DrawRectangle(&pen, 20, 20, 600, 440);
        });
}

mwfl::EnhancedMetafileResult RecordDiagram() noexcept {
    return mwfl::RecordEnhancedMetafile(nullptr, L"diagram", [](HDC dc) {
        ::Rectangle(dc, 0, 0, 640, 480);
    });
}
