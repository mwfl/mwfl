# Hello

This compiled example demonstrates **smallest native application**. It is intentionally small
enough to study from the source while still using real native Windows controls,
messages, and ownership rules.

![Hello example running on Windows](../../docs/images/examples/hello.png)

## What it demonstrates

- `WindowBase`
- `RunApplication`

The window remains a real HWND and the example preserves mwfl's native escape
hatch. UI objects stay on their creating thread, and no exception is allowed to
cross a Win32 callback.

## Key code

The following excerpt is quoted directly from [`main.cpp`](main.cpp):

```cpp
class MainWindow final : public mwfl::WindowBase {
public:
    void BuildUI() override {
        mwfl::Must(SetTitle(L"mwfl hello"), "set window title");
        mwfl::ControlHost ui{*this};
        ui.Add(
            message_,
            L"Hello from a native Windows UI with modern C++20 ergonomics.");
        ui.Add(button_, L"Make it brighter");
        SetLayout(
            mwfl::Column()
                .Margin(28.0_dip)
                .Gap(20.0_dip)
                .Add(message_, mwfl::Fixed(34.0_dip))
                .Add(button_, mwfl::Fixed(36.0_dip), {
                    .alignment = mwfl::CrossAlignment::start,
                    .preferred_size = mwfl::SizeDip{180.0_dip, 36.0_dip},
                }));
```

Read the complete implementation in [`main.cpp`](main.cpp).

## Build and run

From a Visual Studio developer PowerShell at the repository root:

```powershell
cmake --preset vs2026-x64
cmake --build --preset vs2026-x64-debug --target mwfl_hello
build\presets\vs2026-x64\examples\hello\Debug\mwfl_hello.exe
```

Visual Studio 2022 remains supported; substitute the `vs2022-x64` configure and
build presets when validating that toolchain.

## Validation

This example is compiled in both Debug and Release and participates in the example catalog checks. Run `./scripts/verify-change.ps1 -Execute` after modifying it so
the repository selects the additional checks required by the changed paths.
