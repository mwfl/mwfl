# Controls

This compiled example demonstrates **core native control gallery**. It is intentionally small
enough to study from the source while still using real native Windows controls,
messages, and ownership rules.

![Controls example running on Windows](../../docs/images/examples/controls.png)

## What it demonstrates

- `ControlHost`
- `NativeControl`

The window remains a real HWND and the example preserves mwfl's native escape
hatch. UI objects stay on their creating thread, and no exception is allowed to
cross a Win32 callback.

## Key code

The following excerpt is quoted directly from [`main.cpp`](main.cpp):

```cpp
class ControlsWindow final : public mwfl::WindowBase {
public:
    void BuildUI() override {
        SetTitle(L"Modern native controls");
        mwfl::ControlHost ui{*this};
        ui.Add(heading_, L"Native controls, modern C++20 ownership");
        ui.Add(name_label_, L"Your name");
        ui.Add(name_, L"mwfl developer");
        ui.Add(greet_, L"Say hello");
        ui.Add(enabled_, L"Keep the native button enabled");
        ui.Add(accent_);
        ui.Add(progress_);
        ui.Add(status_, L"Ready — the UI remains native HWNDs");
        ui.Add(choices_, L"Choice controls");
        ui.Add(sky_, L"Sky blue");
        ui.Add(cosmos_, L"Cosmic violet");
        ui.Add(items_);
        ui.Add(volume_);
```

Read the complete implementation in [`main.cpp`](main.cpp).

## Build and run

From a Visual Studio developer PowerShell at the repository root:

```powershell
cmake --preset vs2026-x64
cmake --build --preset vs2026-x64-debug --target mwfl_controls_demo
build\presets\vs2026-x64\examples\controls\Debug\mwfl_controls_demo.exe
```

Visual Studio 2022 remains supported; substitute the `vs2022-x64` configure and
build presets when validating that toolchain.

## Validation

This example is compiled in both Debug and Release and participates in the example catalog checks. Run `./scripts/verify-change.ps1 -Execute` after modifying it so
the repository selects the additional checks required by the changed paths.
