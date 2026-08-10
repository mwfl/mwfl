#include <mwfl/dpi.h>

#include <array>
#include <climits>
#include <cstdio>
#include <limits>

namespace {

bool Expect(bool condition, const char* description) {
    if (!condition) {
        std::fprintf(stderr, "DPI assertion failed: %s\n", description);
    }
    return condition;
}

}  // namespace

int main() {
    using namespace mwfl;
    struct Case {
        UINT dpi;
        int expected_100_dip;
    };
    constexpr std::array<Case, 6> cases{{
        {96, 100}, {120, 125}, {144, 150},
        {168, 175}, {192, 200}, {240, 250},
    }};

    bool passed = true;
    for (const Case& item : cases) {
        const DpiContext context = DpiContext::FromDpi(item.dpi);
        passed &= Expect(context.ToPixels(100.0_dip) == item.expected_100_dip,
                         "100 DIP conversion");
        passed &= Expect(context.FromPixels(item.expected_100_dip) == 100.0_dip,
                         "pixel round trip");
    }

    const DpiContext dpi120 = DpiContext::FromDpi(120);
    passed &= Expect(dpi120.ToPixels(0.4_dip) == 1, "positive rounding");
    passed &= Expect(dpi120.ToPixels(-0.4_dip) == -1, "negative rounding");
    const RECT rect = dpi120.ToPixels(
        RectDip{{-10.0_dip, -20.0_dip}, {100.0_dip, 80.0_dip}});
    passed &= Expect(rect.left == -13 && rect.top == -25,
                     "negative monitor coordinates");
    passed &= Expect(rect.right == 112 && rect.bottom == 75,
                     "rectangle size conversion");
    const RECT concise_rect = dpi120.ToPixels(
        RectDip{-10.0_dip, -20.0_dip, 100.0_dip, 80.0_dip});
    passed &= Expect(
        concise_rect.left == rect.left && concise_rect.top == rect.top &&
        concise_rect.right == rect.right && concise_rect.bottom == rect.bottom,
        "concise rectangle construction");
    passed &= Expect(DpiContext::FromDpi(0).GetDpi() == 96,
                     "zero DPI fallback");
    passed &= Expect(DpiContext::FromDpi(UINT_MAX).ToPixels(Dip(1.0e20f)) == INT_MAX,
                     "positive overflow clamp");
    passed &= Expect(DpiContext::FromDpi(UINT_MAX).ToPixels(Dip(-1.0e20f)) == INT_MIN,
                     "negative overflow clamp");
    passed &= Expect(
        DpiContext{}.ToPixels(Dip((std::numeric_limits<float>::quiet_NaN)())) == 0,
        "NaN conversion fallback");
    passed &= Expect(
        DpiContext{}.ToPixels(Dip((std::numeric_limits<float>::infinity)())) == INT_MAX,
        "infinity clamp");
    const RECT saturated = DpiContext::FromDpi(UINT_MAX).ToPixels(
        RectDip{{Dip(1.0e20f), Dip(-1.0e20f)}, {Dip(1.0e20f), Dip(-1.0e20f)}});
    passed &= Expect(saturated.right == INT_MAX && saturated.bottom == INT_MIN,
                     "rectangle edge overflow clamp");
    return passed ? 0 : 1;
}
