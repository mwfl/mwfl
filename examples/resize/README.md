# Resize

This compiled example demonstrates **resize and window state**. It is intentionally small
enough to study from the source while still using real native Windows controls,
messages, and ownership rules.

![Resize example running on Windows](../../docs/images/examples/resize.png)

## What it demonstrates

- `SizeEvent`

The window remains a real HWND and the example preserves mwfl's native escape
hatch. UI objects stay on their creating thread, and no exception is allowed to
cross a Win32 callback.

## Key code

The following excerpt is quoted directly from [`main.cpp`](main.cpp):

```cpp
class ResizeWindow final : public mwfl::WindowBase {
public:
    void BuildUI() override {
        if (!SetTitle(L"Resize demo — resize this window")) {
            throw std::runtime_error("SetTitle failed");
        }
    }

    mwfl::EventResult OnResize(const mwfl::ResizeEvent& event) override {
        wchar_t title[128]{};
        _snwprintf_s(title, _countof(title), _TRUNCATE,
                     L"WM_SIZE: %ld × %ld",
                     event.client_size.cx, event.client_size.cy);
        SetTitle(title);
        return mwfl::EventResult::Propagate();
    }
};
```

Read the complete implementation in [`main.cpp`](main.cpp).

## Build and run

From a Visual Studio developer PowerShell at the repository root:

```powershell
cmake --preset vs2026-x64
cmake --build --preset vs2026-x64-debug --target mwfl_resize_demo
build\presets\vs2026-x64\examples\resize\Debug\mwfl_resize_demo.exe
```

Visual Studio 2022 remains supported; substitute the `vs2022-x64` configure and
build presets when validating that toolchain.

## Validation

This example is compiled in both Debug and Release and participates in the example catalog checks. Run `./scripts/verify-change.ps1 -Execute` after modifying it so
the repository selects the additional checks required by the changed paths.
