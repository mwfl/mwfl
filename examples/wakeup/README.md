# Wakeup

This compiled example demonstrates **worker-to-UI notification**. It is intentionally small
enough to study from the source while still using real native Windows controls,
messages, and ownership rules.

![Wakeup example running on Windows](../../docs/images/examples/wakeup.png)

## What it demonstrates

- `WindowWakeup`

The window remains a real HWND and the example preserves mwfl's native escape
hatch. UI objects stay on their creating thread, and no exception is allowed to
cross a Win32 callback.

## Key code

The following excerpt is quoted directly from [`main.cpp`](main.cpp):

```cpp
class WakeWindow final : public mwfl::WindowBase {
public:
    void BuildUI() override {
        SetTitle(L"Worker thread will wake this HWND safely");
        const mwfl::WindowWakeup wake = GetWakeup();
        worker_ = std::jthread([wake](std::stop_token stop) {
            using namespace std::chrono_literals;
            if (!stop.stop_requested()) {
                std::this_thread::sleep_for(1s);
            }
            if (!stop.stop_requested()) {
                wake.TryWake();
            }
        });
    }

    mwfl::EventResult OnWakeup() noexcept override {
        SetTitle(L"Safe cross-thread wake-up received");
```

Read the complete implementation in [`main.cpp`](main.cpp).

## Build and run

From a Visual Studio developer PowerShell at the repository root:

```powershell
cmake --preset vs2026-x64
cmake --build --preset vs2026-x64-debug --target mwfl_wakeup_demo
build\presets\vs2026-x64\examples\wakeup\Debug\mwfl_wakeup_demo.exe
```

Visual Studio 2022 remains supported; substitute the `vs2022-x64` configure and
build presets when validating that toolchain.

## Validation

This example is compiled in both Debug and Release and participates in the example catalog checks. Run `./scripts/verify-change.ps1 -Execute` after modifying it so
the repository selects the additional checks required by the changed paths.
