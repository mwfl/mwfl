# Appearance

This compiled example demonstrates **theme backdrop and accessibility**. It is intentionally small
enough to study from the source while still using real native Windows controls,
messages, and ownership rules.

![Appearance example running on Windows](../../docs/images/examples/appearance.png)

## What it demonstrates

- `AppearanceOptions`
- `SetAccessibleName`

The window remains a real HWND and the example preserves mwfl's native escape
hatch. UI objects stay on their creating thread, and no exception is allowed to
cross a Win32 callback.

## Key code

The following excerpt is quoted directly from [`main.cpp`](main.cpp):

```cpp
class AppearanceWindow final : public mwfl::WindowBase {
public:
    void BuildUI() override {
        SetTitle(L"Appearance lab");
        mwfl::ControlHost ui{*this};
        ui.Add(title_, L"Appearance lab");
        ui.Add(subtitle_, L"Explore Windows 11 title-bar color, backdrop material, corner policy, and accessibility helpers.");
        ui.Add(settings_, L"Window composition");
        ui.Add(mode_label_, L"Color mode");
        ui.Add(mode_);
        ui.Add(backdrop_label_, L"Backdrop material");
        ui.Add(backdrop_);
        ui.Add(rounded_, L"Prefer rounded corners");
        ui.Add(apply_, L"Apply appearance");
        ui.Add(preview_, L"Live preview");
        ui.Add(preview_title_, L"A thin, native layer");
        ui.Add(preview_body_, L"mwfl asks DWM for modern window composition while keeping normal Win32 controls and explicit HWND ownership.");
        ui.Add(system_, L"");
```

Read the complete implementation in [`main.cpp`](main.cpp).

## Build and run

From a Visual Studio developer PowerShell at the repository root:

```powershell
cmake --preset vs2026-x64
cmake --build --preset vs2026-x64-debug --target mwfl_appearance_demo
build\presets\vs2026-x64\examples\appearance\Debug\mwfl_appearance_demo.exe
```

Visual Studio 2022 remains supported; substitute the `vs2022-x64` configure and
build presets when validating that toolchain.

## Validation

This example is compiled in both Debug and Release and participates in the example catalog checks. Run `./scripts/verify-change.ps1 -Execute` after modifying it so
the repository selects the additional checks required by the changed paths.
