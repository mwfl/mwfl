# Wait Aware

This compiled example demonstrates **kernel handles and idle callbacks**. It is intentionally small
enough to study from the source while still using real native Windows controls,
messages, and ownership rules.

![Wait Aware example running on Windows](../../docs/images/examples/wait-aware.png)

## What it demonstrates

- `MessagePump`

The window remains a real HWND and the example preserves mwfl's native escape
hatch. UI objects stay on their creating thread, and no exception is allowed to
cross a Win32 callback.

## Key code

The following excerpt is quoted directly from [`main.cpp`](main.cpp):

```cpp
class WaitWindow final : public mwfl::WindowBase {
public:
    void BuildUI() override {
        demo_window = GetHwnd();
        SetTitle(L"Wait-aware pump - idle ticks update this title");
    }
};

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
    using namespace std::chrono_literals;
    mwfl::WaitAwareMessagePump pump({
        .idle_interval = 500ms,
        .on_idle = [] {
        ++idle_ticks;
        wchar_t title[96]{};
        ::swprintf_s(title, L"Wait-aware pump - idle tick %u", idle_ticks);
        ::SetWindowTextW(demo_window, title);
        },
```

Read the complete implementation in [`main.cpp`](main.cpp).

## Build and run

From a Visual Studio developer PowerShell at the repository root:

```powershell
cmake --preset vs2026-x64
cmake --build --preset vs2026-x64-debug --target mwfl_wait_aware_demo
build\presets\vs2026-x64\examples\wait_aware\Debug\mwfl_wait_aware_demo.exe
```

Visual Studio 2022 remains supported; substitute the `vs2022-x64` configure and
build presets when validating that toolchain.

## Validation

This example is compiled in both Debug and Release and participates in the example catalog checks. Run `./scripts/verify-change.ps1 -Execute` after modifying it so
the repository selects the additional checks required by the changed paths.
