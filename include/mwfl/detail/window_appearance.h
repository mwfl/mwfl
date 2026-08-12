#pragma once

#include <windows.h>

#include <mwfl/appearance.h>

namespace mwfl::detail {

// Internal state holder used by the public Window template. It owns no HWND.
class WindowAppearance final {
public:
    WindowAppearance() noexcept;
    ~WindowAppearance() noexcept;
    WindowAppearance(const WindowAppearance&) = delete;
    WindowAppearance& operator=(const WindowAppearance&) = delete;

    void Configure(AppearanceOptions options) noexcept;
    bool Set(HWND window, AppearanceOptions options) noexcept;
    bool Reapply(HWND window) noexcept;
    bool IsApplying() const noexcept { return applying_; }
    const AppearanceState& GetState() const noexcept { return state_; }
    bool HandleColorMessage(HWND window, UINT message, WPARAM wparam,
                            LRESULT& result) noexcept;

private:
    void ResetBrushes() noexcept;

    AppearanceOptions options_{};
    AppearanceState state_{};
    HBRUSH window_brush_ = nullptr;
    HBRUSH control_brush_ = nullptr;
    bool applying_ = false;
};

}  // namespace mwfl::detail
