# Build and extend the Scintilla Code Editor

This tutorial targets Windows 10 or later, Visual Studio 2026, MSVC C++20,
and x64. Scintilla is an optional pinned component and is not fetched by core
`mwfl::mwfl` consumers.

## 1. Build and run

```powershell
cmake --preset vs2026-x64-scintilla
cmake --build --preset vs2026-x64-scintilla-debug --target mwfl_code_editor_demo
./build/presets/vs2026-x64-scintilla/examples/code_editor/Debug/mwfl_code_editor_demo.exe
```

CMake verifies the official Scintilla 5.6.5 source and x64 runtime archives.
The target copies `Scintilla.dll` beside the executable. If it is absent,
`ScintillaRuntime::LoadAdjacent` returns a structured status and the reference
application explains that `mwfl_deploy_scintilla(target)` is required.

## 2. Link and deploy

```cmake
find_package(mwfl CONFIG REQUIRED COMPONENTS scintilla)
add_executable(my_editor WIN32 main.cpp)
target_link_libraries(my_editor PRIVATE mwfl::scintilla)
mwfl_deploy_scintilla(my_editor)
```

For source consumption, configure mwfl with `MWFL_BUILD_SCINTILLA=ON` and use
the same target and deployment helper.

## 3. Load before creating the control

```cpp
mwfl::ScintillaRuntime runtime_;
mwfl::ScintillaEditor editor_;

mwfl::Must(runtime_.LoadAdjacent(), "load Scintilla.dll");
ui.AddNative(editor_, mwfl::ControlId{600}, mwfl::RectDip{}, runtime_);
mwfl::Must(editor_.ConfigureCodeEditing(), "configure editor");
```

The runtime state is shared with created editors, so destroying the lightweight
runtime wrapper cannot unload code behind a live HWND. Editor HWND operations,
notifications, and raw `Send` calls stay on the creating UI thread.

## 4. Text, positions, and dirty state

Public strings are strict UTF-16 and are converted to strict UTF-8. A failed
conversion returns `false` or `nullopt`; malformed input is never silently
replaced. `ScintillaPosition` and `ScintillaTextRange` are UTF-8 byte offsets,
not UTF-16 character indexes.

After loading a file, call `SetText`, then `SetSavePoint`, then record
`DocumentState::MarkOpened`. On `save_point_left`, mark the document changed;
after a successful atomic file write, call `SetSavePoint` and
`MarkSavedAs`. Never mark the model saved before I/O succeeds.

## 5. Test without dialogs

```powershell
ctest --test-dir build/presets/vs2026-x64-scintilla -C Debug `
  -R "mwfl.(scintilla_text|scintilla_native|code_editor_gui)"
```

The GUI self-test creates a local Unicode source file, opens it, finds and
replaces text, observes dirty/save-point state, saves and reads it back, checks
undo/redo and zoom, removes the file, and exits without user input.
