# Build a real Notepad application

This tutorial starts from the supported `templates/basic-app` project and ends
with a Unicode, DPI-aware SDI editor. The complete compiled result is
`examples/notepad/main.cpp`; copy from it when a step says “use the canonical
implementation” instead of guessing an API.

## 1. Install the tools

Use Windows 10 1809 or newer and install either Visual Studio 2026 or Visual
Studio 2022 with:

- Desktop development with C++;
- MSVC C++ x64 build tools;
- C++ CMake tools for Windows;
- a current Windows 10 or Windows 11 SDK; and
- Git for Windows.

Open **Developer PowerShell** for the selected Visual Studio and verify:

```powershell
cmake --version
git --version
cl
```

`cl` should print its version and then complain that no source file was given.
That final message is expected.

## 2. Copy the supported template

From a local mwfl checkout:

```powershell
Copy-Item -Recurse .\templates\basic-app ..\mwfl-notepad-tutorial
Set-Location ..\mwfl-notepad-tutorial
```

The template already supplies a Per-Monitor-V2 manifest, C++20 compile options,
and either pinned `FetchContent` consumption or a local-source override.

Configure with the matching generator:

```powershell
# Visual Studio 2026
cmake -S . -B build -G "Visual Studio 18 2026" -A x64 `
  -DMWFL_SOURCE_DIR=D:/GitHub/mwfl

# Visual Studio 2022 alternative
cmake -S . -B build-vs2022 -G "Visual Studio 17 2022" -A x64 `
  -DMWFL_SOURCE_DIR=D:/GitHub/mwfl
```

Build and run the unchanged starter before editing it:

```powershell
cmake --build build --config Debug
& .\build\Debug\mwfl_basic_app.exe
```

If this step fails, fix the toolchain first. Do not diagnose application code
until the untouched template builds.

## 3. Replace the starter controls with the document surface

Store every retained object as a window member. Add:

```cpp
mwfl::DocumentState document_;
mwfl::TextHistory history_;
mwfl::CommandSet commands_;
mwfl::Menu menu_;
mwfl::AcceleratorTable accelerators_;
mwfl::UiFont font_;
mwfl::Toolbar toolbar_;
mwfl::TextBox editor_;
mwfl::StatusBar status_;
```

In `BuildUI`, use `ControlHost` to create the three native controls. Give the
text box `ES_MULTILINE`, scrolling, and `ES_NOHIDESEL`. Use intrinsic sizing for
the bars and let the editor stretch:

```cpp
SetLayout(mwfl::Column()
    .Add(toolbar_, mwfl::Auto())
    .Add(editor_, mwfl::Stretch())
    .Add(status_, mwfl::Auto()));
```

Assign accessible names to surfaces without visible labels:

```cpp
mwfl::Must(mwfl::SetAccessibleName(editor_.GetHwnd(), L"Document text"),
           "name editor");
mwfl::Must(mwfl::SetAccessibleName(status_.GetHwnd(), L"Document status"),
           "name status");
```

The canonical example also updates its system message font from
`OnDpiChanged`, and refreshes it after `WM_SETTINGCHANGE` or `WM_THEMECHANGED`.
Keep system colors so Windows High Contrast remains authoritative.

## 4. Define actions once

Give every action one stable `ControlId`. Put its text, enabled/checked/visible
state, shortcut, and callback in one `Command`:

```cpp
commands_.Add(mwfl::Command(kSave, L"&Save", [this] {
    static_cast<void>(SaveDocument(false));
}).SetShortcut({FVIRTKEY | FCONTROL, 'S'}));
```

Project the same command into the menu and toolbar, then build accelerators from
the set:

```cpp
toolbar_.AddCommand(*commands_.Find(kSave));
file_menu.AppendCommand(*commands_.Find(kSave));
accelerators_.Create(commands_);
SetAccelerators(accelerators_.GetHandle());
```

After document state changes, update the `Command`, then call
`Toolbar::UpdateCommand` and `Menu::UpdateCommand`. Do not put separate save
callbacks in each surface.

## 5. Open and save without losing text

Use `ShowOpenFileDialog` and `ShowSaveFileDialog`. Check the three outcomes
separately:

```cpp
const auto selected = mwfl::ShowOpenFileDialog(options);
if (selected.Cancelled()) return;
if (!selected.accepted) {
    // Report selected.extended_error. Keep the current document.
    return;
}
```

Read with `ReadTextFile`. Only call `document_.MarkOpened(path)` after the read
succeeds. Save with `WriteTextFileAtomic`, passing the `FileStamp` from the read
when overwriting the same path. Only mark the document saved after the atomic
write succeeds. A `changed` result means another process modified the file;
preserve the editor text and ask the user what to do.

The complete encoding and error mapping is in `examples/notepad/main.cpp` and is
covered by `mwfl.text_file`.

## 6. Protect destructive transitions

Before New, Open, close-window, or Exit, call
`DocumentState::EvaluateTransition`. If saving is required, ask the user for
Save, Discard, or Cancel. Cancel returns immediately without clearing the edit
control. Save proceeds only when `SaveDocument` succeeds.

Do not clear the editor before asking, and do not turn a cancelled Save As dialog
into permission to close.

## 7. Add editor workflows

- Send `WM_CUT`, `WM_COPY`, and `WM_PASTE` to the edit HWND.
- Use `TextHistory` for bounded Undo/Redo and saved-position tracking.
- Use `FindTextMatch`, `TextMatchesAt`, and `ReplaceAllText` behind the native
  modeless Find/Replace dialog.
- Call `EnableFileDrop` and handle `WM_DROPFILES` through `ReadDroppedFiles`.
- Use `RecentFileList` for bounded, case-insensitive recent paths.
- Use `SingleInstance` to forward an optional file path to the primary window.

Each operation is already present in the canonical example. Reuse its ordering
and failure checks rather than copying only the happy path.

## 8. Add one command and persistent preference

Follow [the focused Agent recipe](../recipes/notepad-command-setting.md) to add
the checked **Always on Top** command. The setting lives below the versioned key
`Software\\mwfl\\Notepad\\1` as a `REG_DWORD`; a failed optional preference
write never affects document safety.

## 9. Build and verify

For a local checkout, build the canonical application and run its real hidden
GUI workflow:

```powershell
cmake --preset vs2026-x64
cmake --build --preset x64-debug --target mwfl_notepad
ctest --preset x64-debug -R "mwfl.notepad_gui" --output-on-failure
```

The GUI test launches the actual executable and covers open, user-style edit
notifications, atomic save, cancel, discard, reopen, command propagation,
accessible names, theme refresh, DPI font replacement, 50 repeated document
cycles, GUI-resource growth, and close.

Finally run the broader checks selected for application changes:

```powershell
.\scripts\verify-change.ps1 -Base HEAD~1
```

## 10. Produce the distributable executable

Install or package a Release build:

```powershell
cmake --build --preset x64-release
cmake --install build\presets\vs2026-x64 --config Release `
  --prefix build\install
& .\build\install\bin\mwfl_notepad.exe

cpack --config build\presets\vs2026-x64\CPackConfig.cmake `
  -C Release -B build\packages
```

The ZIP contains `bin/mwfl_notepad.exe` together with the library, headers,
CMake package files, licenses, and documentation.

