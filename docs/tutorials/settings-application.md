# Build a persistent Settings application

This tutorial builds the tested application in `examples/property_sheet`. It
uses Visual Studio 2026, MSVC, C++20, and x64 for the primary local workflow.
Visual Studio 2022 remains a supported compatibility generator.

## 1. Verify the repository build

Open **Developer PowerShell for VS 2026** at the mwtl checkout and run:

```powershell
cmake --preset vs2026-x64
cmake --build --preset vs2026-x64-debug --target mwtl_property_sheet_demo
& .\build\presets\vs2026-x64\examples\property_sheet\Debug\mwtl_property_sheet_demo.exe
```

The host window should show the committed profile summary. Select **Open
settings** to open a resizable, modeless native property sheet.

## 2. Copy the supported starter

Create a separate tutorial project from the maintained C++20/Per-Monitor-V2
template, then copy the canonical implementation so every later edit starts
from a compiling baseline:

```powershell
Copy-Item -Recurse .\templates\basic-app ..\mwtl-settings-tutorial
Copy-Item .\examples\property_sheet\main.cpp, `
  .\examples\property_sheet\settings_model.cpp, `
  .\examples\property_sheet\settings_model.h `
  ..\mwtl-settings-tutorial
Set-Location ..\mwtl-settings-tutorial
```

In `CMakeLists.txt`, change the executable sources to:

```cmake
add_executable(mwtl_basic_app WIN32
    main.cpp settings_model.cpp settings_model.h app.manifest)
```

Configure it against the checkout and run it before making changes:

```powershell
cmake -S . -B build -G "Visual Studio 18 2026" -A x64 `
  -DMWTL_SOURCE_DIR=D:/GitHub/mwtl
cmake --build build --config Debug
& .\build\Debug\mwtl_basic_app.exe
```

## 3. Keep application state outside HWNDs

Start with a small value object. Controls are an editable projection; they are
not the source of truth:

```cpp
struct Settings {
    std::wstring display_name = L"Ada Lovelace";
    bool show_notifications = true;
};
```

Keep one `committed_` value in the host window. Build a candidate copy in each
Apply callback and replace `committed_` only after persistence succeeds.

## 4. Add versioned persistence

Use a per-user key such as `Software\\YourCompany\\YourApp\\Settings\\1`.
Store a `SchemaVersion` plus explicitly typed values. Return a structured result
that distinguishes missing data, access denial, invalid data, and I/O failure.
Reject unknown versions and unexpected registry types instead of silently
overwriting them. `settings_model.cpp` is the complete implementation.

The root `HKEY` is borrowed: the helper opens and closes only its child key.
Never delete or close `HKEY_CURRENT_USER`.

## 5. Create the profile page

Give every page a stable nonzero ID. Create controls in `initialize`, then attach
the ordinary mwtl layout to the page:

```cpp
pages.emplace_back(mwtl::PropertyPageOptions{
    {1}, L"Profile",
    {.initialize = [&](HWND page) {
         mwtl::ControlHost ui{page};
         ui.Add(name_label, {101}, L"Display name", {});
         ui.Add(name, {102}, committed.display_name, {});
         return pages[0].SetLayout(mwtl::Column()
             .Margin(16.0_dip).Gap(8.0_dip)
             .Add(name_label, mwtl::Fixed(24.0_dip))
             .Add(name, mwtl::Fixed(34.0_dip)));
     }});
```

In `command`, match `EN_CHANGE` and call `SetDirty()`. In `validate`, reject a
blank or oversized name, show `ShowTaskDialog`, focus the text box, and return
`PropertyPageValidation::invalid`. The sheet stays open and Apply remains
available for correction.

## 6. Apply and reset predictably

The Apply callback copies committed state, reads only this page's controls, and
saves the candidate. Return `false` on a write error so the sheet reports
failure and does not clear dirty state:

```cpp
.apply = [&](HWND page) {
    auto candidate = committed;
    candidate.display_name = name.GetText();
    return Commit(page, std::move(candidate));
},
.reset = [&](HWND) { name.SetText(committed.display_name); },
```

Use the same pattern for the notifications page. Apply commits dirty pages; OK
applies and closes; Cancel calls reset and closes without treating edits as
saved.

## 7. Own the modeless lifetime

Store pages before the `PropertySheetDialog`; page callbacks capture the host
and therefore must not outlive it. If the sheet already exists, activate it
instead of creating another. In the host's `OnClose`, call `sheet.Close()` and
then propagate the close event.

All modeless sheet operations remain on the creating UI thread. `GetHwnd()` is
a borrowed escape hatch for native `PSM_*` messages, not an ownership transfer.

## 8. Run the focused evidence

```powershell
cmake --build --preset vs2026-x64-debug `
  --target mwtl_settings_application_model_test mwtl_property_sheet_demo
ctest --test-dir build/presets/vs2026-x64 -C Debug `
  -R "^mwtl[.](settings_application_model|settings_application_gui)$" `
  --output-on-failure
```

The model test performs missing/save/load/malformed-schema checks against a
unique HKCU test key and deletes it. The GUI test launches the real application,
edits both pages, presses Apply, reads persisted state, checks the host summary,
presses Cancel, and removes its test key.

Before shipping an edit, also run the full VS2026 Debug and Release suites:

```powershell
ctest --preset vs2026-x64-debug
cmake --build --preset vs2026-x64-release
ctest --preset vs2026-x64-release
```
