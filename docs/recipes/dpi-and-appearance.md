# Handle DPI and appearance

Express layout in DIPs and use `GetDpiContext()` for native measurements. Apply
DPI-dependent fonts during setup and refresh them from `OnDpiChanged()`, then
return `Propagate()` so the window completes normal DPI processing.

Use `ApplyWindowAppearanceBestEffort` only for a borrowed raw HWND. Its `true`
result means the request was accepted, not that every DWM attribute took effect;
High Contrast and older Windows versions can override or reject composition
choices. Provide accessible names for controls whose visible label is
insufficient.

For a `WindowBase`, prefer `WindowOptions::appearance` at startup and
`SetAppearance` for an in-app choice. MWFL retains this policy and reapplies it
to the frame, attached menu, native descendants, and customer-area palette when
Windows broadcasts a theme change. Override `OnAppearanceChanged` only to rebuild
application-owned drawing resources; return `Propagate()` unless the underlying
native theme message is intentionally consumed.

```cpp
mwfl::EventResult OnAppearanceChanged(const mwfl::AppearanceState& state) override {
    chart_colors_ = MakeChartColors(state.palette);
    chart_.Invalidate();
    return mwfl::EventResult::Propagate();
}
```

Theme state and HWNDs stay on the creating UI thread. Palette values are copied
plain values; MWFL does not transfer ownership of an `HBRUSH`, render target, or
third-party theme object. Native visual-style calls are best-effort, so custom or
third-party controls must consume the palette through their own supported API.

Canonical implementations: `examples/dpi/main.cpp` and
`examples/appearance/main.cpp`.

