# Commands

This compiled example demonstrates **menu toolbar and shortcut actions**. It is intentionally small
enough to study from the source while still using real native Windows controls,
messages, and ownership rules.

![Commands example running on Windows](../../docs/images/examples/commands.png)

## What it demonstrates

- `CommandSet`
- `Toolbar`
- `Menu`

The window remains a real HWND and the example preserves mwfl's native escape
hatch. UI objects stay on their creating thread, and no exception is allowed to
cross a Win32 callback.

## Key code

The following excerpt is quoted directly from [`main.cpp`](main.cpp):

```cpp
class CommandsWindow final : public mwfl::WindowBase {
public:
    void BuildUI() override {
        SetTitle(L"Command studio - one action model, three surfaces");
        BuildCommands();

        mwfl::ControlHost ui{*this};
        ui.Add(title_, L"Command studio");
        ui.Add(subtitle_, L"Menu, toolbar, and keyboard shortcuts share the same command state.");
        ui.Add(toolbar_);
        mwfl::TextBoxOptions editor_options;
        editor_options.style |= ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL;
        ui.Add(editor_, L"Write something here, then save it.\r\n\r\nCtrl+N creates a fresh note. Ctrl+S saves. F11 toggles focus mode.", editor_options);
        ui.Add(inspector_, L"Command state");
        ui.Add(save_state_, L"Save is disabled until the document changes.");
        ui.Add(focus_state_, L"Focus mode: off");
        ui.Add(hint_, L"Try the toolbar, the Command menu, or the shortcuts - every route dispatches through CommandSet.");
        ui.Add(status_, L"Ready");
```

Read the complete implementation in [`main.cpp`](main.cpp).

## Build and run

From a Visual Studio developer PowerShell at the repository root:

```powershell
cmake --preset vs2026-x64
cmake --build --preset vs2026-x64-debug --target mwfl_commands_demo
build\presets\vs2026-x64\examples\commands\Debug\mwfl_commands_demo.exe
```

Visual Studio 2022 remains supported; substitute the `vs2022-x64` configure and
build presets when validating that toolchain.

## Validation

This example is compiled in both Debug and Release and participates in the example catalog checks. Run `./scripts/verify-change.ps1 -Execute` after modifying it so
the repository selects the additional checks required by the changed paths.
