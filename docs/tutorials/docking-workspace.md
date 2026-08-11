# Build an IDE-style docking workspace

This tutorial builds and explains the complete `docking_workspace` reference
application. It targets Windows 10 or newer, C++20, Visual Studio 2026, MSVC,
and x64. The focused commands below are the local edit loop; the 0.1 release
gate additionally covers VS2022 compatibility, ARM64, sanitizers, coverage,
and hosted CI.

## 1. Configure, build, and run

From a Developer PowerShell prompt at the repository root:

```powershell
cmake --preset vs2026-x64
cmake --build --preset vs2026-x64-debug --target mwfl_docking_workspace_demo
./build/presets/vs2026-x64/examples/docking_workspace/Debug/mwfl_docking_workspace_demo.exe
```

The window starts with two document tabs, Solution Explorer on the left, and
Output below the documents. Try these operations:

1. Drag the Output tab over Solution Explorer and release inside the preview.
2. Begin another drag and press Escape; the original layout is retained.
3. Use **Workspace > Float Output**, then close the floating tool window to
   redock it.
4. Use **Auto-hide Explorer** and **Pin Explorer**.
5. Press Ctrl+K, move with arrows or Tab, press Enter to accept, or Escape to
   cancel.
6. Press Ctrl+Shift+R to reorder the document tabs. Press Ctrl+W to close
   README, then use **Reset Layout** to restore the stable panel identity.
7. Save the layout, close the application, and reopen it.

Run its deterministic offline integration test with:

```powershell
ctest --test-dir build/presets/vs2026-x64 -C Debug --output-on-failure `
  -R "mwfl.docking_workspace_gui"
```

## 2. Give logical objects stable identities

Panel, group, split-node, and floating-host IDs must be nonzero and stable
across runs. They are application data, not HWND values:

```cpp
constexpr mwfl::DockPanelId kDocument{1};
constexpr mwfl::DockPanelId kOutput{2};
constexpr mwfl::DockGroupId kDocuments{10};
constexpr mwfl::DockGroupId kTools{20};

mwfl::DockLayoutModel model{kDocuments, mwfl::DockNodeId{100},
                            mwfl::DockGroupRole::document};
mwfl::Must(static_cast<bool>(model.AddDockedGroup(
    kTools, mwfl::DockNodeId{200}, mwfl::DockGroupRole::tool,
    kDocuments, mwfl::DockNodeId{300}, mwfl::DockEdge::bottom, 0.72)),
    "add tool group");
mwfl::Must(static_cast<bool>(model.AddPanel(
    {kDocument, L"main.cpp", mwfl::DockPanelRole::document}, kDocuments)),
    "add document");
mwfl::Must(static_cast<bool>(model.AddPanel(
    {kOutput, L"Output", mwfl::DockPanelRole::tool}, kTools)),
    "add output");
```

The model owns metadata copies only. Your application continues to own editor,
tree, list, and other content state. Never put an HWND, pointer, callback, or
process-local identifier into a saved layout.

## 3. Bind borrowed HWNDs

Create group-host child windows and panel child windows first. Then attach one
native adapter on the creating UI thread:

```cpp
mwfl::DockNativeWorkspaceAdapter native;
mwfl::Must(static_cast<bool>(native.Attach(main_window)), "attach adapter");
mwfl::Must(static_cast<bool>(native.BindGroup(kDocuments, document_host)),
           "bind document host");
mwfl::Must(static_cast<bool>(native.BindGroup(kTools, tool_host)),
           "bind tool host");
mwfl::Must(static_cast<bool>(native.BindPanel(kDocument, editor)),
           "bind editor");
mwfl::Must(static_cast<bool>(native.BindPanel(kOutput, output_list)),
           "bind output");
mwfl::Must(static_cast<bool>(native.Synchronize(model.GetSnapshot())),
           "project initial layout");
```

The adapter borrows all bound HWNDs. They must be valid, same-process,
same-UI-thread windows. Detaching the adapter does not destroy them.

## 4. Mutate with propose, adopt, commit

Do not move native children first and hope the model catches up. Propose the
logical change, prepare and adopt native moves, commit the model, and finally
commit the adoption:

```cpp
auto mutation = mwfl::MakePinDockMutation(kOutput, kDocuments);
auto transaction = model.Propose(mutation);
if (!transaction) return false;

auto adoption = native.Prepare(transaction->proposed);
if (!adoption || !native.Adopt(*adoption)) return false;
if (!model.Commit(*transaction)) {
    native.Rollback(*adoption);
    return false;
}
if (!native.Commit(*adoption)) return false;
```

`Propose` is non-mutating. A stale transaction, invalid graph, role mismatch,
failed adoption, or callback failure therefore cannot silently discard the
source layout. Keep all of this work on the creating UI thread.

## 5. Compose a cancellable pointer drag

Use `DockDragSession` to separate hit testing, proposal, preview, and commit:

1. Call `Begin(model, panel)` only after the pointer exceeds the system drag
   threshold and capture succeeds.
2. Express destinations as screen-space `DockDropTarget` rectangles in DIPs.
3. Call `UpdateTarget`, create a mutation for the returned target, and pass the
   model proposal to `SetProposal`.
4. Show `DockPreviewWindow`; it is nonactivating and input-transparent.
5. On button release, call `Commit` with native adoption and rollback handlers.
6. On Escape, capture loss, source/target destruction, or shutdown, call
   `Cancel` and hide the preview.

The reference app implements this sequence directly in
`examples/docking_workspace/main.cpp`. `mwfl.docking_drag` covers stale,
throwing, rejected, reentrant, and cancelled transactions; the GUI self-test
covers successful mouse docking and cancellation without layout mutation.

## 6. Provide a keyboard-equivalent workflow

Drag targets are also inputs to `DockKeyboardSession`. Begin with the panel and
targets, move spatially with arrows or sequentially with Tab/Shift+Tab, announce
the selected target, and accept with Enter. Escape cancels. Keep focus on a
stable application window while the nonactivating preview is visible.

This is not merely an accessibility fallback: it gives automation and users a
deterministic operation that does not depend on pointer coordinates.

## 7. Float, auto-hide, and recover monitors

`DockFloatingWindow` owns its auxiliary top-level HWND but borrows its content
host. The floating window is owned by the coordinated main window, restores the
content parent during teardown, applies per-monitor DPI changes, and never posts
`WM_QUIT` for the process.

Store floating placement as bounded virtual-screen DIPs plus a monitor device
name. Before showing restored state, call `RecoverDockFloatingPlacement` with
the current monitor work areas. It clamps unreachable state after monitor
removal or resolution changes and does not require a second physical monitor in
tests.

`DockAutoHideController` is a deterministic state machine. Feed pointer/focus
events and elapsed time into it, then project its decisions into your host. Pin
the panel through a normal model transaction. Never make hover the only way to
open or pin a panel.

## 8. Save and restore safely

Use `SaveDockingSessionAtomic` and `LoadDockingSession`. The format is
versioned, size/depth/count bounded, pointer-free, tolerant of unknown optional
fields, and rejects malformed or cyclic graphs. Save only after a committed
layout. On load:

1. Resolve stable panel IDs against tools installed in this application.
2. Recover floating placement against current monitors.
3. Prepare native adoption for the candidate snapshot.
4. Replace the model only if adoption succeeds.
5. Fall back to a deterministic built-in layout on any failure.

Do not hold an I/O lock while calling application panel resolvers or UI code.

## 9. Shut down in ownership order

Stop active drag and auto-hide sessions, hide/destroy previews, restore and
destroy floating hosts, detach the native adapter, remove subclasses, and only
then let application-owned panel/group HWNDs be destroyed. Save the last fully
committed layout, not a live proposal.

## 10. Diagnose failures

- `DockLayoutStatus` identifies invalid IDs, missing objects, role mismatch,
  invalid graphs, and stale transactions.
- `DockNativeStatus` distinguishes invalid HWNDs, wrong process/thread,
  parentage problems, stale windows, and native call failure.
- `DockDragCommitStatus` distinguishes invalid state, adoption failure,
  callback failure, and logical commit failure.
- Session results distinguish I/O, version, bounds, parse, and graph failures.

Preserve these typed outcomes at subsystem boundaries. Convert a required
startup failure to `mwfl::Must` only where the application truly cannot proceed.

## Verification gate

For the local x64 scope, run both configurations:

```powershell
cmake --build build/presets/vs2026-x64 --config Debug
ctest --test-dir build/presets/vs2026-x64 -C Debug --output-on-failure
cmake --build build/presets/vs2026-x64 --config Release
ctest --test-dir build/presets/vs2026-x64 -C Release --output-on-failure
```

These local commands do not replace the broader release matrix recorded in
`docs/release-readiness.md`.
