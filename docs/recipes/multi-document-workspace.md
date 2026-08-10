# Multi-document workspace recipe

Canonical compiled sources:

- `examples/document_workspace/main.cpp` — complete two-window application;
- `tests/document_workspace_test.cpp` — ordering, history, identity, transfer;
- `tests/document_coordination_test.cpp` — active routing and close plans;
- `tests/document_session_test.cpp` — bounded persistence and restore;
- `tests/document_tabs_native_test.cpp` — HWND projection and reparenting.

## Change map

| Goal | Public surface | Required invariant |
|---|---|---|
| Add a document type | `WorkspaceDocument`, application ID map | model never owns contents or requires a base class |
| Route active Save/Undo | `BuildActiveDocumentCommandProjection`, `RouteActiveDocument` | resolve a stable ID at invocation time |
| Independent undo state | `SetUndoState` | undo buffer remains application-owned per ID |
| Closable/reorderable tab | `Move`, `Close`, `DocumentTabWorkspaceAdapter` | native item data contains an integer ID, never a pointer |
| Move between windows | `TransferDocumentWithPage` | destination accepts before source removal; rollback on failure |
| Recently closed | `RestoreRecentlyClosed`, `ReopenRecentlyClosed` | bounded deterministic order; validate missing/duplicate paths |
| Session restore | `SaveDocumentSessionAtomic`, `RestoreWorkspaceSession` | bounded versioned input; callbacks outside I/O/model locks |
| Coordinated shutdown | `BuildCoordinatedClosePlan`, `ExecuteCoordinatedClose` | gather every decision and finish saves before any close commit |

## Negative rules

- Do not store a `Document*`, view pointer, or `this` in a tab item.
- Do not use a process-global “current document”.
- Do not force application documents to derive from a framework class.
- Do not remove source ownership before destination adoption succeeds.
- Do not write a session directly over the last good file.
- Do not retain model pointers/spans across mutations.
- Do not destroy page HWNDs before adapters are detached or unbound.
- Do not close the first workspace while another workspace may still cancel.

Verify with:

```powershell
cmake --build --preset vs2026-x64-debug --target mwfl_document_workspace
ctest --preset vs2026-x64-debug -R '^mwfl\.(document_workspace|document_coordination|document_session|document_tabs_native|document_workspace_gui)$' --output-on-failure
```
