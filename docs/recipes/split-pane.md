# Add a keyboard-operable split pane

Use `Splitter` when two application-owned child views must share a resizable
region. The splitter owns its container HWND, while the two pane wrappers remain
members of the window and are borrowed by `AttachPanes`.

## 1. Store the wrappers

```cpp
mwfl::Splitter splitter_;
mwfl::TreeView navigation_;
mwfl::ListView contents_;
```

The declaration order makes the panes destruct first. Normally the top-level
window destroys the whole native child tree before C++ member destruction, but
this order also keeps the borrowing relationship obvious to a reviewer or
coding agent.

## 2. Create panes under the splitter

```cpp
void BuildUI() override {
    using mwfl::operator""_dip;

    mwfl::ControlHost ui{*this};
    ui.Add(splitter_, {200}, mwfl::RectDip{}, mwfl::SplitterOptions{
        .constraints = {160.0_dip, 240.0_dip, 6.0_dip},
        .initial_position = 260.0_dip,
    });

    mwfl::ControlHost panes{splitter_};
    panes.Add(navigation_, {201}, mwfl::RectDip{});
    panes.Add(contents_, {202}, mwfl::RectDip{});
    mwfl::Must(splitter_.AttachPanes(
        navigation_.GetHwnd(), contents_.GetHwnd()), "attach split panes");

    SetLayout(mwfl::Column().Add(splitter_, mwfl::Stretch()));
}
```

The normal retained layout sizes the splitter container in DIPs. Its `WM_SIZE`
path then arranges both pane HWNDs, so no second anchor-based layout system is
introduced. Do not create panes under the top-level window and then reparent
them; `AttachPanes` deliberately rejects non-child HWNDs.

The container forwards `WM_NOTIFY`, control-originated `WM_COMMAND`, and
`WM_CONTEXTMENU` from either attached pane to its own parent. A virtual
ListView can therefore remain a direct splitter child while its
`LVN_GETDISPINFO` reaches the top-level `OnNotify`; do not add a second manual
forwarder in the application.

## 3. Observe position changes

```cpp
mwfl::EventResult OnNotify(const mwfl::NotifyEvent& event) override {
    if (!event.Is(splitter_, mwfl::kSplitterPositionChanged)) {
        return mwfl::EventResult::Propagate();
    }
    const auto& changed =
        reinterpret_cast<const mwfl::SplitterNotification&>(event.header);
    SaveSplitPosition(changed.position);
    return mwfl::EventResult::Handled();
}
```

The position is a DIP value. Restoring it with `SetPosition` returns `false` if
the native host or an attached pane became invalid. Users can drag the bar or
focus it with Tab and use the relevant arrow keys; Home and End select the
bounded extremes. The native MSAA surface exposes a named, focusable slider.

See `examples/explorer/main.cpp`, `tests/splitter_test.cpp`, and
`tests/splitter_native_test.cpp` for the compiled canonical path and its pure,
native, accessibility, mouse, keyboard, and resource-lifetime evidence.
