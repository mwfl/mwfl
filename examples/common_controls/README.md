# Common Controls

This compiled example demonstrates **specialized common controls**. It is intentionally small
enough to study from the source while still using real native Windows controls,
messages, and ownership rules.

![Common Controls example running on Windows](../../docs/images/examples/common-controls.png)

## What it demonstrates

- `TreeView`
- `ListView`
- `Toolbar`

The window remains a real HWND and the example preserves mwfl's native escape
hatch. UI objects stay on their creating thread, and no exception is allowed to
cross a Win32 callback.

## Key code

The following excerpt is quoted directly from [`main.cpp`](main.cpp):

```cpp
class CommonControlsWindow final : public mwfl::WindowBase {
public:
    void BuildUI() override {
        SetTitle(L"Windows Common Controls gallery");
        mwfl::ControlHost ui{*this};
        ui.Add(rebar_, {200}, {16.0_dip, 12.0_dip, 1160.0_dip, 48.0_dip});
        mwfl::ControlHost rebar_ui{rebar_};
        rebar_ui.Add(toolbar_, {201}, {0.0_dip, 0.0_dip, 420.0_dip, 34.0_dip});
        ui.Add(tree_, {202}, {16.0_dip, 76.0_dip, 250.0_dip, 310.0_dip});
        ui.Add(list_, {203}, {282.0_dip, 76.0_dip, 400.0_dip, 180.0_dip},
               mwfl::ListViewOptions{.virtual_data = true});
        ui.Add(header_, {204}, {282.0_dip, 270.0_dip, 400.0_dip, 34.0_dip});
        ui.Add(tabs_, {205}, {282.0_dip, 318.0_dip, 400.0_dip, 68.0_dip});
        ui.Add(combo_ex_, {206}, {704.0_dip, 76.0_dip, 250.0_dip, 150.0_dip});
        ui.Add(date_, {207}, {704.0_dip, 126.0_dip, 250.0_dip, 34.0_dip});
        ui.Add(calendar_, {208}, {970.0_dip, 76.0_dip, 260.0_dip, 210.0_dip});
        ui.Add(hot_key_, {209}, {704.0_dip, 176.0_dip, 250.0_dip, 34.0_dip});
        ui.Add(ip_, {210}, {704.0_dip, 226.0_dip, 250.0_dip, 34.0_dip});
```

Read the complete implementation in [`main.cpp`](main.cpp).

## Build and run

From a Visual Studio developer PowerShell at the repository root:

```powershell
cmake --preset vs2026-x64
cmake --build --preset vs2026-x64-debug --target mwfl_common_controls_demo
build\presets\vs2026-x64\examples\common_controls\Debug\mwfl_common_controls_demo.exe
```

Visual Studio 2022 remains supported; substitute the `vs2022-x64` configure and
build presets when validating that toolchain.

## Validation

This example is compiled in both Debug and Release and participates in the example catalog checks. Run `./scripts/verify-change.ps1 -Execute` after modifying it so
the repository selects the additional checks required by the changed paths.
