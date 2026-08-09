#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace drawing {

struct Point {
    float x = 0;
    float y = 0;
    friend bool operator==(const Point&, const Point&) = default;
};

using Stroke = std::vector<Point>;

// Application-owned, GPU-independent drawing data. Coordinates are DIPs.
class Document final {
public:
    bool BeginStroke(Point point);
    bool AppendPoint(Point point);
    bool EndStroke() noexcept;
    bool Undo() noexcept;
    void Clear() noexcept;

    bool IsDrawing() const noexcept { return drawing_; }
    bool IsDirty() const noexcept { return dirty_; }
    std::size_t GetStrokeCount() const noexcept { return strokes_.size(); }
    const std::vector<Stroke>& GetStrokes() const noexcept { return strokes_; }
    std::wstring ToSvg(float width, float height) const;

private:
    std::vector<Stroke> strokes_;
    bool drawing_ = false;
    bool dirty_ = false;
};

}  // namespace drawing
