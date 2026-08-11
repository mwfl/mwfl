# Minmax

This compiled example demonstrates **minimum tracking size**. It is intentionally small
enough to study from the source while still using real native Windows controls,
messages, and ownership rules.

![Minmax example running on Windows](../../docs/images/examples/minmax.png)

## What it demonstrates

- `WindowMessage`

The window remains a real HWND and the example preserves mwfl's native escape
hatch. UI objects stay on their creating thread, and no exception is allowed to
cross a Win32 callback.

## Key code

The following excerpt is quoted directly from [`main.cpp`](main.cpp):

```cpp
class MinMaxWindow final : public mwfl::WindowBase {
public:
    void BuildUI() override {
        if (!SetTitle(L"WM_GETMINMAXINFO — minimum 480 × 280 pixels")) {
            throw std::runtime_error("SetTitle failed");
        }
    }

    mwfl::EventResult OnMinMaxInfo(mwfl::MinMaxInfoEvent event) override {
        event.info.ptMinTrackSize = {480, 280};
        return mwfl::EventResult::Handled();
    }
};

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
    return mwfl::RunApplication<MinMaxWindow>(instance, show_command);
}
```

Read the complete implementation in [`main.cpp`](main.cpp).

## Build and run

From a Visual Studio developer PowerShell at the repository root:

```powershell
cmake --preset vs2026-x64
cmake --build --preset vs2026-x64-debug --target mwfl_minmax_demo
build\presets\vs2026-x64\examples\minmax\Debug\mwfl_minmax_demo.exe
```

Visual Studio 2022 remains supported; substitute the `vs2022-x64` configure and
build presets when validating that toolchain.

## Validation

This example is compiled in both Debug and Release and participates in the example catalog checks. Run `./scripts/verify-change.ps1 -Execute` after modifying it so
the repository selects the additional checks required by the changed paths.
