# Form Binding

This compiled example demonstrates **validated data-entry form**. It is intentionally small
enough to study from the source while still using real native Windows controls,
messages, and ownership rules.

![Form Binding example running on Windows](../../docs/images/examples/form-binding.png)

## What it demonstrates

- `ValueBinding`
- `ValidationResult`
- `ControlHost`

The window remains a real HWND and the example preserves mwfl's native escape
hatch. UI objects stay on their creating thread, and no exception is allowed to
cross a Win32 callback.

## Key code

The following excerpt is quoted directly from [`main.cpp`](main.cpp):

```cpp
class FormBindingWindow final : public mwfl::WindowBase {
public:
    void BuildUI() override {
        SetTitle(L"Profile settings - live binding and validation");

        mwfl::ControlHost ui{*this};
        ui.Add(title_, L"Profile settings");
        ui.Add(subtitle_, L"Edit native controls, validate once, and keep the model explicit.");
        ui.Add(form_group_, L"Account");
        ui.Add(name_label_, L"Display name");
        ui.Add(name_, L"");
        ui.Add(workspace_label_, L"Default workspace");
        ui.Add(workspace_);
        ui.Add(notifications_, L"Show activity notifications");
        ui.Add(validation_, L"");
        ui.Add(preview_group_, L"Live model preview");
        ui.Add(preview_title_, L"Ada Lovelace");
        ui.Add(preview_detail_, L"Design workspace  |  Notifications on");
```

Read the complete implementation in [`main.cpp`](main.cpp).

## Build and run

From a Visual Studio developer PowerShell at the repository root:

```powershell
cmake --preset vs2026-x64
cmake --build --preset vs2026-x64-debug --target mwfl_form_binding_demo
build\presets\vs2026-x64\examples\form_binding\Debug\mwfl_form_binding_demo.exe
```

Visual Studio 2022 remains supported; substitute the `vs2022-x64` configure and
build presets when validating that toolchain.

## Validation

This example is compiled in both Debug and Release and participates in the example catalog checks. Run `./scripts/verify-change.ps1 -Execute` after modifying it so
the repository selects the additional checks required by the changed paths.
