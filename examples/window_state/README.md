# Window State

This compiled example demonstrates **minimize maximize and restore**. It is intentionally small
enough to study from the source while still using real native Windows controls,
messages, and ownership rules.

![Window State example running on Windows](../../docs/images/examples/window-state.png)

## What it demonstrates

- `SizeEvent`

The window remains a real HWND and the example preserves mwfl's native escape
hatch. UI objects stay on their creating thread, and no exception is allowed to
cross a Win32 callback.

## Key code

The following excerpt is quoted directly from [`main.cpp`](main.cpp):

```cpp
class WindowStateWindow final : public mwfl::WindowBase {
public:
    void BuildUI() override {
        if (!SetTitle(L"Window state demo — minimize or maximize")) {
            throw std::runtime_error("SetTitle failed");
        }
    }

    mwfl::EventResult OnResize(const mwfl::ResizeEvent& event) override {
        const wchar_t* label = L"restored";
        if (event.state == mwfl::WindowSizeState::minimized) label = L"minimized";
        if (event.state == mwfl::WindowSizeState::maximized) label = L"maximized";
        wchar_t title[96]{};
        _snwprintf_s(title, _countof(title), _TRUNCATE,
                     L"Window state: %s", label);
        SetTitle(title);
        return mwfl::EventResult::Propagate();
    }
```

Read the complete implementation in [`main.cpp`](main.cpp).

## Build and run

From a Visual Studio developer PowerShell at the repository root:

```powershell
cmake --preset vs2026-x64
cmake --build --preset vs2026-x64-debug --target mwfl_window_state_demo
build\presets\vs2026-x64\examples\window_state\Debug\mwfl_window_state_demo.exe
```

Visual Studio 2022 remains supported; substitute the `vs2022-x64` configure and
build presets when validating that toolchain.

## Validation

This example is compiled in both Debug and Release and participates in the example catalog checks. Run `./scripts/verify-change.ps1 -Execute` after modifying it so
the repository selects the additional checks required by the changed paths.
