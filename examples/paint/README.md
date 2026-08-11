# Paint

This compiled example demonstrates **native GDI painting**. It is intentionally small
enough to study from the source while still using real native Windows controls,
messages, and ownership rules.

![Paint example running on Windows](../../docs/images/examples/paint.png)

## What it demonstrates

- `PaintEvent`

The window remains a real HWND and the example preserves mwfl's native escape
hatch. UI objects stay on their creating thread, and no exception is allowed to
cross a Win32 callback.

## Key code

The following excerpt is quoted directly from [`main.cpp`](main.cpp):

```cpp
class PaintWindow final : public mwfl::WindowBase {
public:
    void BuildUI() override {
        ::SetWindowPos(GetHwnd(), nullptr, 0, 0, 900, 560,
                       SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        if (!SetTitle(L"Native GDI paint demo")) {
            throw std::runtime_error("SetTitle failed");
        }
    }

    mwfl::EventResult OnPaint(mwfl::PaintEvent& event) override {
        const HDC dc = event.GetDC();
        if (dc != nullptr) {
            RECT client{};
            ::GetClientRect(GetHwnd(), &client);
            ::SetBkMode(dc, TRANSPARENT);
            ::SetTextColor(dc, RGB(24, 78, 119));
            ::DrawTextW(dc,
```

Read the complete implementation in [`main.cpp`](main.cpp).

## Build and run

From a Visual Studio developer PowerShell at the repository root:

```powershell
cmake --preset vs2026-x64
cmake --build --preset vs2026-x64-debug --target mwfl_paint_demo
build\presets\vs2026-x64\examples\paint\Debug\mwfl_paint_demo.exe
```

Visual Studio 2022 remains supported; substitute the `vs2022-x64` configure and
build presets when validating that toolchain.

## Validation

This example is compiled in both Debug and Release and participates in the example catalog checks. Run `./scripts/verify-change.ps1 -Execute` after modifying it so
the repository selects the additional checks required by the changed paths.
