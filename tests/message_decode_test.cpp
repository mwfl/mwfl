#include <mwfl/detail/message_decode.h>

#include <cstdlib>
#include <cstdint>

namespace {

std::uint64_t Next(std::uint64_t& state) noexcept {
    state ^= state << 13;
    state ^= state >> 7;
    state ^= state << 17;
    return state;
}

}  // namespace

int main() {
    constexpr LPARAM key_bits = 3 | (0x2AL << 16) | (1LL << 24) | (1LL << 30);
    constexpr auto key = mwfl::detail::DecodeKeyEvent(VK_SPACE, key_bits);
    static_assert(key.virtual_key == VK_SPACE && key.repeat_count == 3 &&
                  key.scan_code == 0x2A && key.extended && key.was_down);

    constexpr LPARAM mouse_bits =
        static_cast<LPARAM>(static_cast<WORD>(-12)) |
        (static_cast<LPARAM>(static_cast<WORD>(34)) << 16);
    constexpr auto mouse = mwfl::detail::DecodeMouseEvent(MK_CONTROL, mouse_bits);
    static_assert(mouse.position.x == -12 && mouse.position.y == 34 &&
                  mouse.key_state == MK_CONTROL);

    constexpr auto resize = mwfl::detail::DecodeResizeEvent(
        SIZE_MAXIMIZED, MAKELPARAM(640, 480));
    static_assert(resize.client_size.cx == 640 && resize.client_size.cy == 480 &&
                  resize.state == mwfl::WindowSizeState::maximized);

    const HWND source = reinterpret_cast<HWND>(0x1234);
    const auto command = mwfl::detail::DecodeCommandEvent(
        MAKEWPARAM(42, BN_CLICKED), reinterpret_cast<LPARAM>(source));
    if (command.id.value != 42 || command.notification != BN_CLICKED ||
        command.control != source) return EXIT_FAILURE;

    const auto scroll = mwfl::detail::DecodeScrollEvent(
        WM_VSCROLL, MAKEWPARAM(SB_THUMBPOSITION, 17),
        reinterpret_cast<LPARAM>(source));
    if (scroll.control != source || scroll.request != SB_THUMBPOSITION ||
        scroll.position != 17 || scroll.horizontal) return EXIT_FAILURE;

    // Exercise all packed field widths with a fixed seed so failures reproduce.
    std::uint64_t state = 0x4D57544C'0600C0DEULL;
    for (int iteration = 0; iteration < 10000; ++iteration) {
        const auto bits = Next(state);
        const auto x = static_cast<short>(bits & 0xFFFFU);
        const auto y = static_cast<short>((bits >> 16) & 0xFFFFU);
        const auto decoded_mouse = mwfl::detail::DecodeMouseEvent(
            static_cast<WPARAM>(bits >> 32),
            MAKELPARAM(static_cast<WORD>(x), static_cast<WORD>(y)));
        if (decoded_mouse.position.x != x || decoded_mouse.position.y != y ||
            decoded_mouse.key_state != static_cast<WPARAM>(bits >> 32)) {
            return EXIT_FAILURE;
        }

        const WORD id = static_cast<WORD>(bits);
        const WORD notification = static_cast<WORD>(bits >> 16);
        const auto decoded_command = mwfl::detail::DecodeCommandEvent(
            MAKEWPARAM(id, notification), reinterpret_cast<LPARAM>(source));
        if (decoded_command.id.value != id ||
            decoded_command.notification != notification ||
            decoded_command.control != source) return EXIT_FAILURE;

        const WORD request = static_cast<WORD>(bits >> 32);
        const WORD position = static_cast<WORD>(bits >> 48);
        const bool horizontal = (bits & 1U) != 0;
        const auto decoded_scroll = mwfl::detail::DecodeScrollEvent(
            horizontal ? WM_HSCROLL : WM_VSCROLL,
            MAKEWPARAM(request, position), reinterpret_cast<LPARAM>(source));
        if (decoded_scroll.request != request ||
            decoded_scroll.position != position ||
            decoded_scroll.horizontal != horizontal ||
            decoded_scroll.control != source) return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
