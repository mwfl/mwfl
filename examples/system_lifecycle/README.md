# System Lifecycle

This compiled example demonstrates **power display IME and session events**. It is intentionally small
enough to study from the source while still using real native Windows controls,
messages, and ownership rules.

![System Lifecycle example running on Windows](../../docs/images/examples/system-lifecycle.png)

## What it demonstrates

- `WindowMessage`

The window remains a real HWND and the example preserves mwfl's native escape
hatch. UI objects stay on their creating thread, and no exception is allowed to
cross a Win32 callback.

## Key code

The following excerpt is quoted directly from [`main.cpp`](main.cpp):

```cpp
class SystemWindow final : public mwfl::WindowBase {
public:
    void BuildUI() override { SetTitle(L"Native system lifecycle messages"); }

    mwfl::EventResult OnMessage(const mwfl::WindowMessage& message) noexcept override {
        if (message.id == WM_QUERYENDSESSION) {
            return mwfl::EventResult::Handled(TRUE);
        }
        if (message.id == WM_GETOBJECT) {
            // A consumer can return UiaReturnRawElementProvider(...) here.
            return mwfl::EventResult::Propagate();
        }
        const bool lifecycle_message =
            message.id == WM_DISPLAYCHANGE || message.id == WM_SETTINGCHANGE ||
            message.id == WM_POWERBROADCAST ||
            message.id == WM_IME_STARTCOMPOSITION ||
            message.id == WM_IME_COMPOSITION || message.id == WM_ENDSESSION;
        if (!lifecycle_message) {
```

Read the complete implementation in [`main.cpp`](main.cpp).

## Build and run

From a Visual Studio developer PowerShell at the repository root:

```powershell
cmake --preset vs2026-x64
cmake --build --preset vs2026-x64-debug --target mwfl_system_lifecycle_demo
build\presets\vs2026-x64\examples\system_lifecycle\Debug\mwfl_system_lifecycle_demo.exe
```

Visual Studio 2022 remains supported; substitute the `vs2022-x64` configure and
build presets when validating that toolchain.

## Validation

This example is compiled in both Debug and Release and participates in the example catalog checks. Run `./scripts/verify-change.ps1 -Execute` after modifying it so
the repository selects the additional checks required by the changed paths.
