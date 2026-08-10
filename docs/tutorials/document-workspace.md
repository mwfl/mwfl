# Build a multi-document workspace

This tutorial builds and verifies the maintained two-window reference at
`examples/document_workspace`. It targets Windows 10+, C++20, Visual Studio
2026, MSVC, and x64. Document contents remain application-owned throughout.

## 1. Configure the approved local toolchain

Open **Developer PowerShell for VS 2026** at the repository root:

```powershell
cmake --preset vs2026-x64
cmake --build --preset vs2026-x64-debug --target mwtl_document_workspace
ctest --preset vs2026-x64-debug -R '^mwtl\.document_workspace_gui$' --output-on-failure
```

The self-test is offline and non-interactive. It creates two hidden top-level
windows and exercises the real controls. VS2022, ARM64, sanitizers,
and CI are intentionally outside the 0.6 local acceptance gate.

## 2. Separate logical documents from application contents

Use `DocumentWorkspaceModel` for stable IDs, order, active selection, dirty and
undo projection, paths, view state, and recently closed metadata. Store text,
undo buffers, file stamps, and page HWND ownership in the application keyed by
`DocumentId`. Never put a C++ pointer in `TCITEM::lParam`.

```cpp
mwtl::DocumentWorkspaceModel workspace{{1}, 8};
std::unordered_map<std::uint64_t, DocumentContent> contents;
workspace.Add({{42}, L"notes.txt", L"C:\\Docs\\notes.txt"});
contents[42] = {L"application-owned text", stamp};
```

Every ID is nonzero. Paths are deduplicated with Windows case-insensitive,
lexically normalized comparison.

## 3. Project documents into native tabs

Create the `TabControl` and edit pages as siblings with the same workspace
parent. Attach and bind only borrowed HWNDs:

```cpp
mwtl::DocumentTabWorkspaceAdapter tabs;
Must(tabs.Attach(tab_control) == mwtl::DocumentTabStatus::success, "attach tabs");
Must(tabs.BindPage(document_id, editor_hwnd) ==
     mwtl::DocumentTabStatus::success, "bind editor");
Must(tabs.Synchronize(workspace), "project document tabs");
```

Sibling pages allow `EN_CHANGE` and accessibility notifications to reach the
workspace window naturally. Call `ArrangePages` after resize/DPI layout. On
`TCN_SELCHANGE`, call `ActivateNativeSelection`; it reads only the stable ID.

## 4. Route every active-document command explicitly

Build one projection for Save, Close, Undo, and Redo. Application callbacks
resolve the current ID at invocation time instead of retaining a document
pointer:

```cpp
auto projection = mwtl::BuildActiveDocumentCommandProjection(workspace);
Must(mwtl::ApplyActiveDocumentCommandProjection(
         commands, {kSave, kClose, kUndo, kRedo}, projection) ==
     mwtl::DocumentCommandProjectionStatus::success,
     "project commands");

commands.Add(mwtl::Command(kSave, L"Save", [&] {
    mwtl::RouteActiveDocument(workspace, SaveByStableId);
}));
```

Update the menu and toolbar from the same `Command` objects. This prevents a
stale tab callback from saving a newly active document by accident.

## 5. Move a document between top-level windows

Set `WindowOptions::quit_on_destroy = false` for the auxiliary top-level
window. Then use `TransferDocumentWithPage`. It validates both adapters,
accepts metadata in the destination, reparents the page, commits source
removal, and rolls back metadata and HWND parentage on failure.

Application content stays in the process-wide ID map, so no text or undo owner
moves prematurely. After success, synchronize both windows and focus the
destination page.

## 6. Save, close, and reopen safely

Use `ReadTextFile` and `WriteTextFileAtomic`. Retain `FileStamp`; a changed
destination produces `TextFileStatus::changed` and leaves local text dirty.
Closing remembers metadata but must not erase application content if Reopen is
offered. Before reopening a disk-only history item, validate that the path
still exists and has the expected stamp. Already-open paths activate the
existing logical document instead of creating a duplicate.

## 7. Persist and restore sessions

Capture each workspace, save the versioned session atomically, and load it with
explicit limits. Malformed, unsupported, duplicate, truncated, or oversized
state never mutates a workspace.

```cpp
mwtl::DocumentSession session;
session.workspaces.push_back(mwtl::CaptureWorkspaceSession(left));
session.workspaces.push_back(mwtl::CaptureWorkspaceSession(right));
mwtl::SaveDocumentSessionAtomic(session_path, session);
```

Restore into empty matching models with a validator that classifies each path
as `restore`, `missing`, `changed`, `untrusted`, or `rejected`. The application
restore callback creates content owners; callback exceptions are contained and
reported. A malformed load falls back to a safe empty workspace.

## 8. Coordinate shutdown before destroying HWNDs

Collect decisions for every dirty document in both windows before committing
either workspace. A cancel leaves every document and window intact. Run all
saves first, then execute the close plans. Detach adapters, destroy page HWNDs,
destroy the auxiliary window, and finally let the main window post `WM_QUIT`.

## 9. Diagnose failures

- `invalid_parent`: a page does not share the TabControl parent.
- `wrong_thread_or_process`: an HWND crossed its native ownership boundary.
- `stale_workspace`: a callback mutated the model after a plan was built.
- `projection_failure`: logical ownership is valid but native tabs need a full
  resynchronization.
- `TextFileStatus::changed`: preserve local content and ask the user how to
  reconcile the external edit.

Read the complete compiled implementation in
`examples/document_workspace/main.cpp` and the focused recipe in
`docs/recipes/multi-document-workspace.md`.
