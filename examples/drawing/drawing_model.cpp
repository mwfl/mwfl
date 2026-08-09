#include "drawing_model.h"

#include <algorithm>
#include <iomanip>
#include <locale>
#include <sstream>

namespace drawing {

bool Document::BeginStroke(Point point) {
    if (drawing_) return false;
    strokes_.push_back({point});
    drawing_ = true;
    dirty_ = true;
    return true;
}

bool Document::AppendPoint(Point point) {
    if (!drawing_ || strokes_.empty()) return false;
    if (strokes_.back().back() != point) strokes_.back().push_back(point);
    return true;
}

bool Document::EndStroke() noexcept {
    if (!drawing_) return false;
    drawing_ = false;
    return true;
}

bool Document::Undo() noexcept {
    if (drawing_ || strokes_.empty()) return false;
    strokes_.pop_back();
    dirty_ = true;
    return true;
}

void Document::Clear() noexcept {
    if (!strokes_.empty()) dirty_ = true;
    strokes_.clear();
    drawing_ = false;
}

std::wstring Document::ToSvg(float width, float height) const {
    std::wostringstream output;
    output.imbue(std::locale::classic());
    output << std::fixed << std::setprecision(2)
           << L"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
           << L"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\""
           << (std::max)(width, 1.0f) << L"\" height=\"" << (std::max)(height, 1.0f)
           << L"\" viewBox=\"0 0 " << (std::max)(width, 1.0f) << L' '
           << (std::max)(height, 1.0f) << L"\">\n"
           << L"  <rect width=\"100%\" height=\"100%\" fill=\"white\"/>\n";
    for (const auto& stroke : strokes_) {
        if (stroke.empty()) continue;
        output << L"  <polyline fill=\"none\" stroke=\"#2563eb\" stroke-width=\"3\" "
                  L"stroke-linecap=\"round\" stroke-linejoin=\"round\" points=\"";
        for (const Point point : stroke) output << point.x << L',' << point.y << L' ';
        output << L"\"/>\n";
    }
    output << L"</svg>\n";
    return output.str();
}

}  // namespace drawing
