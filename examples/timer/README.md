# Timer

This compiled example demonstrates **RAII UI timer**. It is intentionally small
enough to study from the source while still using real native Windows controls,
messages, and ownership rules.

![Timer example running on Windows](../../docs/images/examples/timer.png)

## What it demonstrates

- `UiTimer`

The window remains a real HWND and the example preserves mwfl's native escape
hatch. UI objects stay on their creating thread, and no exception is allowed to
cross a Win32 callback.

## Key code

The following excerpt is quoted directly from [`main.cpp`](main.cpp):

```cpp
class TimerWindow final : public mwfl::WindowBase {
public:
    void BuildUI() override {
        ::SetWindowPos(GetHwnd(), nullptr, 0, 0, 900, 560,
                       SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        if (!SetTitle(L"Timer demo — waiting for WM_TIMER")) {
            throw std::runtime_error("SetTitle failed");
        }
        if (!timer_.Start(*this, kTimer, 1s)) {
            throw std::runtime_error("UiTimer::Start failed");
        }
    }

    mwfl::EventResult OnTimer(mwfl::TimerId timer_id) override {
        if (timer_id != kTimer) {
            return mwfl::EventResult::Propagate();
        }
        wchar_t title[96]{};
```

Read the complete implementation in [`main.cpp`](main.cpp).

## Build and run

From a Visual Studio developer PowerShell at the repository root:

```powershell
cmake --preset vs2026-x64
cmake --build --preset vs2026-x64-debug --target mwfl_timer_demo
build\presets\vs2026-x64\examples\timer\Debug\mwfl_timer_demo.exe
```

Visual Studio 2022 remains supported; substitute the `vs2022-x64` configure and
build presets when validating that toolchain.

## Validation

This example is compiled in both Debug and Release and participates in the example catalog checks. Run `./scripts/verify-change.ps1 -Execute` after modifying it so
the repository selects the additional checks required by the changed paths.
