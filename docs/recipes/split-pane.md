# Add a keyboard-operable split pane

Use `Splitter` when two application-owned child views must share a resizable
region. The splitter owns its container HWND, while the two pane wrappers remain
members of the window and are borrowed by `AttachPanes`.

## 1. Store the wrappers

```cpp
mwtl::Splitter splitter_;
mwtl::TreeView navigation_;
mwtl::ListView contents_;
```

The declaration order makes the panes destruct first. Normally the top-level
window destroys the whole native child tree before C++ member destruction, but
this order also keeps the borrowing relationship obvious to a reviewer or
coding agent.

## 2. Create panes under the splitter

```cpp
void BuildUI() override {
    using mwtl::operator""_dip;

    mwtl::ControlHost ui{*this};
    ui.Add(splitter_, {200}, mwtl::RectDip{}, mwtl::SplitterOptions{
        .constraints = {160.0_dip, 240.0_dip, 6.0_dip},
        .initial_position = 260.0_dip,
    });

    mwtl::ControlHost panes{splitter_};
    panes.Add(navigation_, {201}, mwtl::RectDip{});
    panes.Add(contents_, {202}, mwtl::RectDip{});
    mwtl::Must(splitter_.AttachPanes(
        navigation_.GetHwnd(), contents_.GetHwnd()), "attach split panes");

    SetLayout(mwtl::Column().Add(splitter_, mwtl::Stretch()));
}
```

The normal retained layout sizes the splitter container in DIPs. Its `WM_SIZE`
path then arranges both pane HWNDs, so no second anchor-based layout system is
introduced. Do not create panes under the top-level window and then reparent
them; `AttachPanes` deliberately rejects non-child HWNDs.

## 3. Observe position changes

```cpp
mwtl::EventResult OnNotify(const mwtl::NotifyEvent& event) override {
    if (!event.Is(splitter_, mwtl::kSplitterPositionChanged)) {
        return mwtl::EventResult::Propagate();
    }
    const auto& changed =
        reinterpret_cast<const mwtl::SplitterNotification&>(event.header);
    SaveSplitPosition(changed.position);
    return mwtl::EventResult::Handled();
}
```

The position is a DIP value. Restoring it with `SetPosition` returns `false` if
the native host or an attached pane became invalid. Users can drag the bar or
focus it with Tab and use the relevant arrow keys; Home and End select the
bounded extremes. The native MSAA surface exposes a named, focusable slider.

See `examples/common_controls/main.cpp`, `tests/splitter_test.cpp`, and
`tests/splitter_native_test.cpp` for the compiled canonical path and its pure,
native, accessibility, mouse, keyboard, and resource-lifetime evidence.
