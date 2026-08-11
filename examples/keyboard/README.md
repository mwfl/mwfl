# Keyboard

This compiled example demonstrates **keyboard input**. It is intentionally small
enough to study from the source while still using real native Windows controls,
messages, and ownership rules.

![Keyboard example running on Windows](../../docs/images/examples/keyboard.png)

## What it demonstrates

- `KeyEvent`

The window remains a real HWND and the example preserves mwfl's native escape
hatch. UI objects stay on their creating thread, and no exception is allowed to
cross a Win32 callback.

## Key code

The following excerpt is quoted directly from [`main.cpp`](main.cpp):

```cpp
class KeyboardWindow final : public mwfl::WindowBase {
public:
    void BuildUI() override {
        if (!SetTitle(L"Keyboard demo — press a key; Escape closes")) {
            throw std::runtime_error("SetTitle failed");
        }
    }

    mwfl::EventResult OnKeyDown(const mwfl::KeyEvent& event) override {
        if (event.virtual_key == VK_ESCAPE) {
            Close();
            return mwfl::EventResult::Handled();
        }
        wchar_t title[96]{};
        _snwprintf_s(title, _countof(title), _TRUNCATE,
                     L"WM_KEYDOWN virtual key: 0x%02llX",
                     static_cast<unsigned long long>(event.virtual_key));
        SetTitle(title);
```

Read the complete implementation in [`main.cpp`](main.cpp).

## Build and run

From a Visual Studio developer PowerShell at the repository root:

```powershell
cmake --preset vs2026-x64
cmake --build --preset vs2026-x64-debug --target mwfl_keyboard_demo
build\presets\vs2026-x64\examples\keyboard\Debug\mwfl_keyboard_demo.exe
```

Visual Studio 2022 remains supported; substitute the `vs2022-x64` configure and
build presets when validating that toolchain.

## Validation

This example is compiled in both Debug and Release and participates in the example catalog checks. Run `./scripts/verify-change.ps1 -Execute` after modifying it so
the repository selects the additional checks required by the changed paths.
