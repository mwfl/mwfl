#pragma once

#include <windows.h>

#include <compare>

namespace mwfl {

struct Dip {
    float value = 0.0f;

    constexpr Dip() noexcept = default;
    constexpr explicit Dip(float input) noexcept : value(input) {}

    constexpr auto operator<=>(const Dip&) const noexcept = default;
};

constexpr Dip operator+(Dip left, Dip right) noexcept {
    return Dip(left.value + right.value);
}

constexpr Dip operator-(Dip left, Dip right) noexcept {
    return Dip(left.value - right.value);
}

constexpr Dip operator-(Dip value) noexcept {
    return Dip(-value.value);
}

struct PointDip {
    Dip x{};
    Dip y{};
};

struct SizeDip {
    Dip width{};
    Dip height{};
};

struct RectDip {
    PointDip origin{};
    SizeDip size{};

    constexpr RectDip() noexcept = default;
    constexpr RectDip(PointDip input_origin, SizeDip input_size) noexcept
        : origin(input_origin), size(input_size) {}
    constexpr RectDip(
        Dip x, Dip y, Dip width, Dip height) noexcept
        : origin{x, y}, size{width, height} {}
};

struct Thickness {
    Dip left{};
    Dip top{};
    Dip right{};
    Dip bottom{};
};

constexpr Dip operator""_dip(long double value) noexcept {
    return Dip(static_cast<float>(value));
}

constexpr Dip operator""_dip(unsigned long long value) noexcept {
    return Dip(static_cast<float>(value));
}

class DpiContext final {
public:
    static constexpr UINT kDefaultDpi = 96;

    constexpr DpiContext() noexcept = default;
    explicit constexpr DpiContext(UINT dpi) noexcept
        : dpi_(dpi == 0 ? kDefaultDpi : dpi) {}

    static DpiContext FromWindow(HWND window) noexcept;
    static constexpr DpiContext FromDpi(UINT dpi) noexcept {
        return DpiContext(dpi);
    }

    constexpr UINT GetDpi() const noexcept { return dpi_; }
    constexpr float GetScale() const noexcept {
        return static_cast<float>(dpi_) / static_cast<float>(kDefaultDpi);
    }

    int ToPixels(Dip value) const noexcept;
    Dip FromPixels(int value) const noexcept;
    POINT ToPixels(PointDip value) const noexcept;
    SIZE ToPixels(SizeDip value) const noexcept;
    RECT ToPixels(RectDip value) const noexcept;

private:
    UINT dpi_ = kDefaultDpi;
};

}  // namespace mwfl
