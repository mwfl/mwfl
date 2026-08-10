#include "../examples/drawing/drawing_model.h"

#include <string>

int main() {
    drawing::Document document;
    if (document.IsDirty() || document.IsDrawing() || document.GetStrokeCount() != 0 ||
        document.AppendPoint({1, 2}) || document.EndStroke() || document.Undo())
        return 1;
    if (!document.BeginStroke({1, 2}) || document.BeginStroke({3, 4}) ||
        !document.AppendPoint({1, 2}) || !document.AppendPoint({3, 4}) ||
        !document.EndStroke())
        return 2;
    if (!document.IsDirty() || document.IsDrawing() || document.GetStrokeCount() != 1 ||
        document.GetStrokes().front().size() != 2)
        return 3;
    const std::wstring svg = document.ToSvg(320, 200);
    if (svg.find(L"viewBox=\"0 0 320.00 200.00\"") == std::wstring::npos ||
        svg.find(L"1.00,2.00 3.00,4.00") == std::wstring::npos)
        return 4;
    if (!document.Undo() || document.GetStrokeCount() != 0 || document.Undo()) return 5;
    document.BeginStroke({5, 6});
    document.EndStroke();
    document.Clear();
    if (document.GetStrokeCount() != 0 || document.IsDrawing()) return 6;
    return 0;
}
