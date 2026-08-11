# Self Drawn Host

This compiled example demonstrates **worker-driven native drawing**. It is intentionally small
enough to study from the source while still using real native Windows controls,
messages, and ownership rules.

![Self Drawn Host example running on Windows](../../docs/images/examples/self-drawn-host.png)

## What it demonstrates

- `WindowWakeup`
- `MessagePump`

The window remains a real HWND and the example preserves mwfl's native escape
hatch. UI objects stay on their creating thread, and no exception is allowed to
cross a Win32 callback.

## Key code

The following excerpt is quoted directly from [`main.cpp`](main.cpp):

```cpp
class SelfDrawnWindow final : public mwfl::WindowBase {
public:
    void BuildUI() override {
        SetTitle(L"Self-drawn host - worker-driven dirty frames");
        const mwfl::WindowWakeup wake = GetWakeup();
        producer_ = std::jthread([wake](std::stop_token stop) {
            while (!stop.stop_requested()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(16));
                if (!wake.TryWake()) break;
            }
        });
    }

    mwfl::EventResult OnWakeup() noexcept override {
        ++frame_;
        ::InvalidateRect(GetHwnd(), nullptr, FALSE);
        return mwfl::EventResult::Handled();
    }
```

Read the complete implementation in [`main.cpp`](main.cpp).

## Build and run

From a Visual Studio developer PowerShell at the repository root:

```powershell
cmake --preset vs2026-x64
cmake --build --preset vs2026-x64-debug --target mwfl_self_drawn_host_demo
build\presets\vs2026-x64\examples\self_drawn_host\Debug\mwfl_self_drawn_host_demo.exe
```

Visual Studio 2022 remains supported; substitute the `vs2022-x64` configure and
build presets when validating that toolchain.

## Validation

This example is compiled in both Debug and Release and participates in the example catalog checks. Run `./scripts/verify-change.ps1 -Execute` after modifying it so
the repository selects the additional checks required by the changed paths.
