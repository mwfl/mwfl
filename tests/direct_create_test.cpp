#include <mwfl/mwfl.h>

#include <concepts>
#include <string_view>

namespace {

template <typename Control>
concept BoundsControl = requires(
    Control& control, HWND parent, mwfl::ControlId id, mwfl::RectDip bounds) {
    { control.Create(parent, id, bounds) } -> std::same_as<bool>;
};

template <typename Control>
concept TextControl = requires(
    Control& control, HWND parent, mwfl::ControlId id,
    std::wstring_view text, mwfl::RectDip bounds) {
    { control.Create(parent, id, text, bounds) } -> std::same_as<bool>;
};

static_assert(TextControl<mwfl::Label>);
static_assert(TextControl<mwfl::Button>);
static_assert(TextControl<mwfl::TextBox>);
static_assert(TextControl<mwfl::CheckBox>);
static_assert(TextControl<mwfl::RadioButton>);
static_assert(TextControl<mwfl::GroupBox>);
static_assert(TextControl<mwfl::SysLink>);

static_assert(BoundsControl<mwfl::ListBox>);
static_assert(BoundsControl<mwfl::ComboBox>);
static_assert(BoundsControl<mwfl::ProgressBar>);
static_assert(BoundsControl<mwfl::Slider>);
static_assert(BoundsControl<mwfl::ScrollBar>);
static_assert(BoundsControl<mwfl::TreeView>);
static_assert(BoundsControl<mwfl::ListView>);
static_assert(BoundsControl<mwfl::Header>);
static_assert(BoundsControl<mwfl::TabControl>);
static_assert(BoundsControl<mwfl::ComboBoxEx>);
static_assert(BoundsControl<mwfl::DateTimePicker>);
static_assert(BoundsControl<mwfl::MonthCalendar>);
static_assert(BoundsControl<mwfl::HotKey>);
static_assert(BoundsControl<mwfl::IpAddress>);
static_assert(BoundsControl<mwfl::UpDown>);
static_assert(BoundsControl<mwfl::Toolbar>);
static_assert(BoundsControl<mwfl::StatusBar>);
static_assert(BoundsControl<mwfl::Rebar>);
static_assert(BoundsControl<mwfl::Pager>);
static_assert(BoundsControl<mwfl::Animation>);

}  // namespace
