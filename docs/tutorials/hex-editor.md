# Build the Hex Editor

The standalone [MWFL Hex Editor](https://github.com/mwfl/hex-editor) demonstrates
an application-owned custom HWND for a data-heavy editor while MWFL supplies the
window lifecycle, controls, responsive layout, dialogs, drag and drop, and system
Light/Dark appearance state.

## Safety model

Files open read-only. Enabling editing requires an explicit warning and permits
only byte overwrite, so a mistaken nibble cannot shift the rest of a structured
file. All edits remain in memory until the user chooses a save operation.

**Save As** writes a new destination. **Save + backup** first rejects an original
that changed since opening, writes a complete sibling temporary, and calls the
Windows `ReplaceFileW` contract to retain the prior source as `<name>.bak`.

## Build and test

```powershell
git clone https://github.com/mwfl/hex-editor.git
cd hex-editor
cmake --preset vs2026-x64
cmake --build --preset vs2026-x64-release
ctest --preset vs2026-x64-release
```

The model test covers parsing, overwrite/undo, search, integer interpretation,
Save As, atomic replacement, and backup contents. The GUI self-test drives the
real custom control through hexadecimal input and verifies repaint under a dark
appearance. The example intentionally excludes disks, process memory, insertion,
deletion, and multi-gigabyte mapping; those require distinct privilege and data
structure contracts.
