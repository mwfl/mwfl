#include <mwfl/dpi.h>

#include <cmath>
#include <limits>

namespace mwfl {
namespace {

int RoundAndClamp(double value) noexcept {
    constexpr double minimum =
        static_cast<double>((std::numeric_limits<int>::min)());
    constexpr double maximum =
        static_cast<double>((std::numeric_limits<int>::max)());
    if (std::isnan(value)) {
        return 0;
    }
    if (value <= minimum) {
        return (std::numeric_limits<int>::min)();
    }
    if (value >= maximum) {
        return (std::numeric_limits<int>::max)();
    }
    return static_cast<int>(std::lround(value));
}

int AddAndClamp(int left, int right) noexcept {
    const long long value = static_cast<long long>(left) + right;
    if (value <= (std::numeric_limits<int>::min)()) {
        return (std::numeric_limits<int>::min)();
    }
    if (value >= (std::numeric_limits<int>::max)()) {
        return (std::numeric_limits<int>::max)();
    }
    return static_cast<int>(value);
}

}  // namespace

DpiContext DpiContext::FromWindow(HWND window) noexcept {
    if (window == nullptr || ::IsWindow(window) == FALSE) {
        return DpiContext{};
    }
    return DpiContext(::GetDpiForWindow(window));
}

int DpiContext::ToPixels(Dip value) const noexcept {
    return RoundAndClamp(
        static_cast<double>(value.value) * static_cast<double>(dpi_) /
        static_cast<double>(kDefaultDpi));
}

Dip DpiContext::FromPixels(int value) const noexcept {
    return Dip(
        static_cast<float>(value) * static_cast<float>(kDefaultDpi) /
        static_cast<float>(dpi_));
}

POINT DpiContext::ToPixels(PointDip value) const noexcept {
    return POINT{ToPixels(value.x), ToPixels(value.y)};
}

SIZE DpiContext::ToPixels(SizeDip value) const noexcept {
    return SIZE{ToPixels(value.width), ToPixels(value.height)};
}

RECT DpiContext::ToPixels(RectDip value) const noexcept {
    const POINT origin = ToPixels(value.origin);
    const SIZE size = ToPixels(value.size);
    return RECT{
        origin.x,
        origin.y,
        AddAndClamp(origin.x, size.cx),
        AddAndClamp(origin.y, size.cy)};
}

}  // namespace mwfl
