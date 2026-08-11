# Window Options

This compiled example demonstrates **styles resources and DIP bounds**. It is intentionally small
enough to study from the source while still using real native Windows controls,
messages, and ownership rules.

![Window Options example running on Windows](../../docs/images/examples/window-options.png)

## What it demonstrates

- `WindowOptions`

The window remains a real HWND and the example preserves mwfl's native escape
hatch. UI objects stay on their creating thread, and no exception is allowed to
cross a Win32 callback.

## Key code

The following excerpt is quoted directly from [`main.cpp`](main.cpp):

```cpp
class OptionsWindow final : public mwfl::Window<OptionsWindow, DemoClassTraits> {
public:
    void BuildUI() { SetTitle(L"Configurable class, style and DIP client size"); }
};

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
    return mwfl::RunApplication<OptionsWindow>(
        instance,
        show_command,
        {
            .title = L"Window options demo",
            .initial_bounds = {{0.0_dip, 0.0_dip}, {800.0_dip, 500.0_dip}},
            .use_default_bounds = false,
            .center_in_work_area = true,
            .bounds_are_client_size = true,
        });
}
```

Read the complete implementation in [`main.cpp`](main.cpp).

## Build and run

From a Visual Studio developer PowerShell at the repository root:

```powershell
cmake --preset vs2026-x64
cmake --build --preset vs2026-x64-debug --target mwfl_window_options_demo
build\presets\vs2026-x64\examples\window_options\Debug\mwfl_window_options_demo.exe
```

Visual Studio 2022 remains supported; substitute the `vs2022-x64` configure and
build presets when validating that toolchain.

## Validation

This example is compiled in both Debug and Release and participates in the example catalog checks. Run `./scripts/verify-change.ps1 -Execute` after modifying it so
the repository selects the additional checks required by the changed paths.
