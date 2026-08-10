#pragma once

#include <windows.h>
#include <commctrl.h>

#include <mwfl/concepts.h>
#include <mwfl/controls.h>
#include <mwfl/dpi.h>

namespace mwfl {

enum class SplitterOrientation {
    vertical,
    horizontal,
};

struct SplitterConstraints {
    Dip minimum_first{80.0f};
    Dip minimum_second{80.0f};
    Dip bar_thickness{6.0f};
};

struct SplitterLayout {
    RectDip first{};
    RectDip bar{};
    RectDip second{};
    bool constraints_satisfied = true;
};

// UI state for a two-pane splitter. Values are DIPs, so callers recompute the
// returned rectangles after a size or DPI change. The model owns no HWNDs and
// may be used off the UI thread when externally synchronized.
class SplitterModel final {
public:
    explicit SplitterModel(SplitterOrientation orientation = SplitterOrientation::vertical,
                           SplitterConstraints constraints = {}) noexcept;

    SplitterOrientation GetOrientation() const noexcept;
    SplitterConstraints GetConstraints() const noexcept;
    Dip GetPosition() const noexcept;

    void SetOrientation(SplitterOrientation orientation) noexcept;
    void SetConstraints(SplitterConstraints constraints) noexcept;
    void SetPosition(Dip position) noexcept;
    void Adjust(Dip delta) noexcept;
    void SetRatio(float ratio, SizeDip extent) noexcept;
    SplitterLayout Arrange(SizeDip extent) noexcept;

private:
    SplitterOrientation orientation_ = SplitterOrientation::vertical;
    SplitterConstraints constraints_{};
    Dip position_{160.0f};
};

inline constexpr UINT kSplitterPositionChanged = NM_FIRST - 80U;

struct SplitterNotification {
    NMHDR header{};
    Dip position{};
};

struct SplitterOptions {
    SplitterOrientation orientation = SplitterOrientation::vertical;
    SplitterConstraints constraints{};
    Dip initial_position{160.0f};
    Dip keyboard_step{8.0f};
    DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPCHILDREN | TBS_NOTICKS;
    DWORD extended_style = WS_EX_CONTROLPARENT;
};

// Owns one native child container and borrows its two pane HWNDs. Attached
// panes must be distinct direct children of the splitter and must remain valid
// until detached or until the splitter is destroyed. All window operations
// belong to the creating UI thread. WM_NOTIFY, control-originated WM_COMMAND,
// and WM_CONTEXTMENU from attached panes are forwarded to the splitter's
// parent with their original parameters and result. Failure returns false and
// sets last-error.
class Splitter final : public NativeControl {
public:
    Splitter() noexcept = default;
    ~Splitter() noexcept;

    Splitter(const Splitter&) = delete;
    Splitter& operator=(const Splitter&) = delete;
    Splitter(Splitter&&) = delete;
    Splitter& operator=(Splitter&&) = delete;

    bool Create(HWND parent, ControlId id, RectDip bounds, SplitterOptions options = {});
    template <WindowLike Parent>
    bool Create(const Parent& parent, ControlId id, RectDip bounds, SplitterOptions options = {}) {
        return Create(parent.GetHwnd(), id, bounds, options);
    }

    bool AttachPanes(HWND first, HWND second) noexcept;
    void DetachPanes() noexcept;
    HWND GetFirstPane() const noexcept;
    HWND GetSecondPane() const noexcept;

    bool SetBounds(RectDip bounds) noexcept;
    bool Arrange() noexcept;
    bool SetPosition(Dip position) noexcept;
    Dip GetPosition() const noexcept;
    bool SetConstraints(SplitterConstraints constraints) noexcept;
    SplitterConstraints GetConstraints() const noexcept;
    SplitterOrientation GetOrientation() const noexcept;

private:
    static LRESULT CALLBACK SubclassProcedure(HWND window, UINT message, WPARAM wparam,
                                              LPARAM lparam, UINT_PTR subclass_id,
                                              DWORD_PTR reference) noexcept;
    LRESULT ProcessMessage(HWND window, UINT message, WPARAM wparam, LPARAM lparam) noexcept;
    void NotifyPositionChanged() noexcept;
    void InvalidateBar() noexcept;

    SplitterModel model_{};
    HWND first_ = nullptr;   // Borrowed direct child.
    HWND second_ = nullptr;  // Borrowed direct child.
    Dip keyboard_step_{8.0f};
    bool dragging_ = false;
    Dip drag_offset_{};
};

}  // namespace mwfl
