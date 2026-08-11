# Mouse

This compiled example demonstrates **mouse input and coordinates**. It is intentionally small
enough to study from the source while still using real native Windows controls,
messages, and ownership rules.

![Mouse example running on Windows](../../docs/images/examples/mouse.png)

## What it demonstrates

- `MouseEvent`

The window remains a real HWND and the example preserves mwfl's native escape
hatch. UI objects stay on their creating thread, and no exception is allowed to
cross a Win32 callback.

## Key code

The following excerpt is quoted directly from [`main.cpp`](main.cpp):

```cpp
class MouseWindow final : public mwfl::WindowBase {
public:
    void BuildUI() override {
        if (!SetTitle(L"Mouse demo — move or click in the client area")) {
            throw std::runtime_error("SetTitle failed");
        }
    }

    mwfl::EventResult OnMouseMove(const mwfl::MouseEvent& event) override {
        ShowPoint(L"WM_MOUSEMOVE", event.position);
        return mwfl::EventResult::Handled();
    }

    mwfl::EventResult OnLeftButtonDown(const mwfl::MouseEvent& event) override {
        ShowPoint(L"WM_LBUTTONDOWN", event.position);
        return mwfl::EventResult::Handled();
    }
```

Read the complete implementation in [`main.cpp`](main.cpp).

## Build and run

From a Visual Studio developer PowerShell at the repository root:

```powershell
cmake --preset vs2026-x64
cmake --build --preset vs2026-x64-debug --target mwfl_mouse_demo
build\presets\vs2026-x64\examples\mouse\Debug\mwfl_mouse_demo.exe
```

Visual Studio 2022 remains supported; substitute the `vs2022-x64` configure and
build presets when validating that toolchain.

## Validation

This example is compiled in both Debug and Release and participates in the example catalog checks. Run `./scripts/verify-change.ps1 -Execute` after modifying it so
the repository selects the additional checks required by the changed paths.
