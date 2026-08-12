#include <mwfl/detail/window_appearance.h>

#include <utility>

namespace mwfl::detail {

WindowAppearance::WindowAppearance() noexcept : state_(ResolveAppearance()) {}
WindowAppearance::~WindowAppearance() noexcept { ResetBrushes(); }

void WindowAppearance::Configure(AppearanceOptions options) noexcept { options_ = options; }

bool WindowAppearance::Set(HWND window, AppearanceOptions options) noexcept {
    options_ = options;
    return Reapply(window);
}

bool WindowAppearance::Reapply(HWND window) noexcept {
    if (applying_) return true;
    applying_ = true;
    state_ = ResolveAppearance(options_);
    ResetBrushes();
    window_brush_ = ::CreateSolidBrush(state_.palette.window_background);
    control_brush_ = ::CreateSolidBrush(state_.palette.control_background);
    const bool applied = window != nullptr && ApplyWindowAppearance(window, options_);
    applying_ = false;
    return applied;
}

bool WindowAppearance::HandleColorMessage(HWND window, UINT message, WPARAM wparam,
                                          LRESULT& result) noexcept {
    if (message == WM_ERASEBKGND && window_brush_ != nullptr) {
        RECT bounds{};
        ::GetClientRect(window, &bounds);
        ::FillRect(reinterpret_cast<HDC>(wparam), &bounds, window_brush_);
        result = 1;
        return true;
    }
    if (message != WM_CTLCOLORSTATIC && message != WM_CTLCOLOREDIT &&
        message != WM_CTLCOLORLISTBOX && message != WM_CTLCOLORBTN &&
        message != WM_CTLCOLORDLG) {
        return false;
    }
    HDC dc = reinterpret_cast<HDC>(wparam);
    ::SetTextColor(dc, state_.palette.text);
    ::SetBkColor(dc, state_.palette.control_background);
    result = reinterpret_cast<LRESULT>(control_brush_ != nullptr ? control_brush_ : window_brush_);
    return result != 0;
}

void WindowAppearance::ResetBrushes() noexcept {
    if (window_brush_ != nullptr) ::DeleteObject(std::exchange(window_brush_, nullptr));
    if (control_brush_ != nullptr) ::DeleteObject(std::exchange(control_brush_, nullptr));
}

}  // namespace mwfl::detail
