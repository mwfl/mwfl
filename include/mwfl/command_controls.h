#pragma once

#include <windows.h>
#include <commctrl.h>

#include <concepts>
#include <span>
#include <ranges>
#include <string_view>
#include <vector>

#include <mwfl/concepts.h>
#include <mwfl/controls.h>
#include <mwfl/command.h>
#include <mwfl/control_resources.h>

namespace mwfl {

struct ToolbarOptions { DWORD style = WS_CHILD | WS_VISIBLE | TBSTYLE_FLAT | TBSTYLE_TOOLTIPS | CCS_NODIVIDER | CCS_NORESIZE | CCS_NOPARENTALIGN; DWORD extended_style = 0; };
class Toolbar final : public NativeControl {
public:
    bool Create(HWND parent, ControlId id, RectDip bounds, ToolbarOptions options = {});
    template <WindowLike Parent>
    bool Create(const Parent& parent, ControlId id, RectDip bounds, ToolbarOptions options = {}) { return Create(parent.GetHwnd(), id, bounds, options); }
    bool AddTextButton(ControlId command, std::wstring_view text);
    bool AddCommand(const Command& command);
    bool UpdateCommand(const Command& command) noexcept;
    bool SetImageList(const ImageList& images) noexcept;
    Toolbar& AutoSize() noexcept;
};

struct StatusBarOptions { DWORD style = WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP; DWORD extended_style = 0; };
class StatusBar final : public NativeControl {
public:
    bool Create(HWND parent, ControlId id, RectDip bounds, StatusBarOptions options = {});
    template <WindowLike Parent>
    bool Create(const Parent& parent, ControlId id, RectDip bounds, StatusBarOptions options = {}) { return Create(parent.GetHwnd(), id, bounds, options); }
    bool SetParts(std::span<const int> right_edges) noexcept;
    template <std::ranges::input_range Range>
        requires std::convertible_to<std::ranges::range_reference_t<Range>, int>
    bool SetParts(Range&& right_edges) {
        std::vector<int> values;
        if constexpr (std::ranges::sized_range<Range>) {
            values.reserve(std::ranges::size(right_edges));
        }
        for (auto&& edge : right_edges) values.push_back(edge);
        return SetParts(std::span<const int>{values});
    }
    bool SetPartText(int part, std::wstring_view text);
};

struct RebarOptions { DWORD style = WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | RBS_VARHEIGHT | CCS_NODIVIDER; DWORD extended_style = WS_EX_TOOLWINDOW; };
class Rebar final : public NativeControl {
public:
    bool Create(HWND parent, ControlId id, RectDip bounds, RebarOptions options = {});
    template <WindowLike Parent>
    bool Create(const Parent& parent, ControlId id, RectDip bounds, RebarOptions options = {}) { return Create(parent.GetHwnd(), id, bounds, options); }
    bool AddBand(const NativeControl& child, std::wstring_view text, int minimum_width);
};

struct PagerOptions { DWORD style = WS_CHILD | WS_VISIBLE | PGS_HORZ; DWORD extended_style = 0; };
class Pager final : public NativeControl {
public:
    bool Create(HWND parent, ControlId id, RectDip bounds, PagerOptions options = {});
    template <WindowLike Parent>
    bool Create(const Parent& parent, ControlId id, RectDip bounds, PagerOptions options = {}) { return Create(parent.GetHwnd(), id, bounds, options); }
    Pager& SetChild(const NativeControl& child) noexcept;
    Pager& SetButtonSize(int pixels) noexcept;
};

struct AnimationOptions { DWORD style = WS_CHILD | WS_VISIBLE | ACS_CENTER | ACS_TRANSPARENT; DWORD extended_style = 0; };
class Animation final : public NativeControl {
public:
    bool Create(HWND parent, ControlId id, RectDip bounds, AnimationOptions options = {});
    template <WindowLike Parent>
    bool Create(const Parent& parent, ControlId id, RectDip bounds, AnimationOptions options = {}) { return Create(parent.GetHwnd(), id, bounds, options); }
    bool Open(HINSTANCE module, UINT resource_id) noexcept;
    bool Play(UINT repeat_count = UINT_MAX, UINT from = 0, UINT to = UINT_MAX) noexcept;
    Animation& Stop() noexcept;
};

}  // namespace mwfl
