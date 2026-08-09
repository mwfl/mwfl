# Document-style commands and desktop integration

Canonical source: `examples/notepad/main.cpp`. The smaller
`examples/commands/main.cpp` and `examples/desktop_integration/main.cpp` remain
focused supporting examples.

The Notepad reference application demonstrates the conventional native
document-window surface:
one command model shared by menu, toolbar, and keyboard shortcuts; editable
content and dirty state; open/save/folder dialogs; clipboard; file drops; task
dialogs; and persistent window placement.

## Composition

1. Give every action one stable `ControlId` and one callback.
2. Route command events through `CommandSet` after handling control
   notifications.
3. Synchronize enabled/checked state to each native presentation.
4. Parent desktop dialogs to the application HWND.
5. Treat cancellation separately from failure.
6. Capture placement in `OnClose()` and then propagate normal closing.

Use this shape when generating editors, viewers, small IDE-like tools, and
document utilities. `mwtl.notepad_gui` drives command/accelerator routing,
MSAA names, theme refresh, responsive geometry, resource bounds, and real file
state; the focused model suites prove encoding, atomic save, search, history,
recent files, and single-instance activation without HWNDs.
