# DPI

This compiled example demonstrates **per-monitor DPI behavior**. It is intentionally small
enough to study from the source while still using real native Windows controls,
messages, and ownership rules.

![DPI example running on Windows](../../docs/images/examples/dpi.png)

## What it demonstrates

- `DpiContext`
- `DpiChangedEvent`

The window remains a real HWND and the example preserves mwfl's native escape
hatch. UI objects stay on their creating thread, and no exception is allowed to
cross a Win32 callback.

## Key code

The following excerpt is quoted directly from [`main.cpp`](main.cpp):

```cpp
class DpiWindow final : public mwfl::WindowBase {
public:
    void BuildUI() override { UpdateTitle(GetDpiContext()); }

    mwfl::EventResult OnDpiChanged(const mwfl::DpiChangedEvent& event) noexcept override {
        UpdateTitle(mwfl::DpiContext::FromDpi(event.dpi_x));
        return mwfl::EventResult::Propagate();
    }

private:
    void UpdateTitle(mwfl::DpiContext dpi) noexcept {
        wchar_t title[96]{};
        ::swprintf_s(title, L"DPI demo - %u DPI (%.2fx)", dpi.GetDpi(), dpi.GetScale());
        SetTitle(title);
    }

};
```

Read the complete implementation in [`main.cpp`](main.cpp).

## Build and run

From a Visual Studio developer PowerShell at the repository root:

```powershell
cmake --preset vs2026-x64
cmake --build --preset vs2026-x64-debug --target mwfl_dpi_demo
build\presets\vs2026-x64\examples\dpi\Debug\mwfl_dpi_demo.exe
```

Visual Studio 2022 remains supported; substitute the `vs2022-x64` configure and
build presets when validating that toolchain.

## Validation

This example is compiled in both Debug and Release and participates in the example catalog checks. Run `./scripts/verify-change.ps1 -Execute` after modifying it so
the repository selects the additional checks required by the changed paths.
