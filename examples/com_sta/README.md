# COM Sta

This compiled example demonstrates **COM STA lifecycle**. It is intentionally small
enough to study from the source while still using real native Windows controls,
messages, and ownership rules.

![COM Sta example running on Windows](../../docs/images/examples/com-sta.png)

## What it demonstrates

- `ApplicationOptions`
- `ComApartment`

The window remains a real HWND and the example preserves mwfl's native escape
hatch. UI objects stay on their creating thread, and no exception is allowed to
cross a Win32 callback.

## Key code

The following excerpt is quoted directly from [`main.cpp`](main.cpp):

```cpp
class ComWindow final : public mwfl::WindowBase {
public:
    void BuildUI() override {
        APTTYPE type{};
        APTTYPEQUALIFIER qualifier{};
        const bool sta = SUCCEEDED(::CoGetApartmentType(&type, &qualifier)) &&
            (type == APTTYPE_STA || type == APTTYPE_MAINSTA);
        SetTitle(sta ? L"COM STA initialized by mwfl::Application"
                     : L"Unexpected COM apartment");
    }
};

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
    return mwfl::RunApplication<ComWindow>(
        instance,
        show_command,
        {},
        {.com_apartment = mwfl::ComApartment::sta});
```

Read the complete implementation in [`main.cpp`](main.cpp).

## Build and run

From a Visual Studio developer PowerShell at the repository root:

```powershell
cmake --preset vs2026-x64
cmake --build --preset vs2026-x64-debug --target mwfl_com_sta_demo
build\presets\vs2026-x64\examples\com_sta\Debug\mwfl_com_sta_demo.exe
```

Visual Studio 2022 remains supported; substitute the `vs2022-x64` configure and
build presets when validating that toolchain.

## Validation

This example is compiled in both Debug and Release and participates in the example catalog checks. Run `./scripts/verify-change.ps1 -Execute` after modifying it so
the repository selects the additional checks required by the changed paths.
