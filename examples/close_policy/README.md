# Close Policy

This compiled example demonstrates **intercept and propagate close**. It is intentionally small
enough to study from the source while still using real native Windows controls,
messages, and ownership rules.

![Close Policy example running on Windows](../../docs/images/examples/close-policy.png)

## What it demonstrates

- `OnClose`
- `EventResult`

The window remains a real HWND and the example preserves mwfl's native escape
hatch. UI objects stay on their creating thread, and no exception is allowed to
cross a Win32 callback.

## Key code

The following excerpt is quoted directly from [`main.cpp`](main.cpp):

```cpp
class ClosePolicyWindow final : public mwfl::WindowBase {
public:
    void BuildUI() override {
        if (!SetTitle(L"Close policy demo — close twice to exit")) {
            throw std::runtime_error("SetTitle failed");
        }
    }

    mwfl::EventResult OnClose() override {
        if (!close_confirmed_) {
            close_confirmed_ = true;
            SetTitle(L"Close requested once — close again to confirm");
            return mwfl::EventResult::Handled();
        }
        return mwfl::EventResult::Propagate();
    }

private:
```

Read the complete implementation in [`main.cpp`](main.cpp).

## Build and run

From a Visual Studio developer PowerShell at the repository root:

```powershell
cmake --preset vs2026-x64
cmake --build --preset vs2026-x64-debug --target mwfl_close_policy_demo
build\presets\vs2026-x64\examples\close_policy\Debug\mwfl_close_policy_demo.exe
```

Visual Studio 2022 remains supported; substitute the `vs2022-x64` configure and
build presets when validating that toolchain.

## Validation

This example is compiled in both Debug and Release and participates in the example catalog checks. Run `./scripts/verify-change.ps1 -Execute` after modifying it so
the repository selects the additional checks required by the changed paths.
