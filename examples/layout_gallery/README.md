# Layout Gallery

This compiled example demonstrates **responsive nested native layout**. It is intentionally small
enough to study from the source while still using real native Windows controls,
messages, and ownership rules.

![Layout Gallery example running on Windows](../../docs/images/examples/layout-gallery.png)

## What it demonstrates

- `Row`
- `Column`
- `Overlay`

The window remains a real HWND and the example preserves mwfl's native escape
hatch. UI objects stay on their creating thread, and no exception is allowed to
cross a Win32 callback.

## Key code

The following excerpt is quoted directly from [`main.cpp`](main.cpp):

```cpp
class LayoutGalleryWindow final : public mwfl::WindowBase {
public:
    void BuildUI() override {
        SetTitle(L"Responsive layout gallery");
        mwfl::ControlHost ui{*this};
        ui.Add(title_, L"Responsive layout gallery");
        ui.Add(subtitle_, L"Resize the window and switch density to see nested Row, Column, Overlay, Auto, Fixed, and Stretch behavior.");
        ui.Add(compact_, L"Compact");
        ui.Add(comfortable_, L"Comfortable");
        ui.Add(long_text_, L"Toggle long content");
        ui.Add(profile_, L"Profile card - Overlay");
        ui.Add(avatar_, L"AL");
        ui.Add(name_, L"Ada Lovelace");
        ui.Add(role_, L"Computing pioneer");
        ui.Add(active_, L"Active");
        ui.Add(metrics_, L"Weighted stretch row");
        ui.Add(metric_a_, L"12\r\nProjects");
        ui.Add(metric_b_, L"48\r\nReviews");
```

Read the complete implementation in [`main.cpp`](main.cpp).

## Build and run

From a Visual Studio developer PowerShell at the repository root:

```powershell
cmake --preset vs2026-x64
cmake --build --preset vs2026-x64-debug --target mwfl_layout_gallery_demo
build\presets\vs2026-x64\examples\layout_gallery\Debug\mwfl_layout_gallery_demo.exe
```

Visual Studio 2022 remains supported; substitute the `vs2022-x64` configure and
build presets when validating that toolchain.

## Validation

This example is compiled in both Debug and Release and participates in the example catalog checks. Run `./scripts/verify-change.ps1 -Execute` after modifying it so
the repository selects the additional checks required by the changed paths.
