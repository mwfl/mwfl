# Desktop Integration

This compiled example demonstrates **modal custom and shell dialogs, clipboard, drops, and placement**. It is intentionally small
enough to study from the source while still using real native Windows controls,
messages, and ownership rules.

![Desktop Integration example running on Windows](../../docs/images/examples/desktop-integration.png)

## What it demonstrates

- `Dialog`
- `DialogResult`
- `FileDialogOptions`
- `SavedWindowPlacement`

The window remains a real HWND and the example preserves mwfl's native escape
hatch. UI objects stay on their creating thread, and no exception is allowed to
cross a Win32 callback.

## Key code

The following excerpt is quoted directly from [`main.cpp`](main.cpp):

```cpp
class DesktopIntegrationWindow final : public mwfl::WindowBase {
   public:
    void BuildUI() override {
        SetTitle(L"Desktop integration toolbox");
        mwfl::ControlHost ui{*this};
        ui.Add(title_, L"Desktop integration");
        ui.Add(subtitle_,
               L"Modern dialogs, clipboard, drag and drop, and persistent placement in one native "
               L"window.");
        ui.Add(dialogs_, L"Files and folders");
        ui.Add(open_, L"Open file...");
        ui.Add(save_, L"Save as...");
        ui.Add(folder_, L"Choose folder...");
        ui.Add(clipboard_, L"Clipboard");
        ui.Add(copy_, L"Copy summary");
        ui.Add(paste_, L"Paste text");
        ui.Add(task_, L"Show task dialog");
        ui.Add(custom_, L"Show custom dialog");
```

Read the complete implementation in [`main.cpp`](main.cpp).

## Build and run

From a Visual Studio developer PowerShell at the repository root:

```powershell
cmake --preset vs2026-x64
cmake --build --preset vs2026-x64-debug --target mwfl_desktop_integration_demo
build\presets\vs2026-x64\examples\desktop_integration\Debug\mwfl_desktop_integration_demo.exe
```

Visual Studio 2022 remains supported; substitute the `vs2022-x64` configure and
build presets when validating that toolchain.

## Validation

The focused validation targets are `mwfl.dialog_native`, `mwfl.desktop`. Run `./scripts/verify-change.ps1 -Execute` after modifying it so
the repository selects the additional checks required by the changed paths.
