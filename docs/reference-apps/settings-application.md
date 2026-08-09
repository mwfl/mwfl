# Settings application

Canonical source: `examples/property_sheet/main.cpp` with the independently
testable state and persistence layer in `examples/property_sheet/settings_model.*`.

This is a complete native settings workflow rather than a control screenshot.
It uses two resizable property pages, stable page IDs, dirty tracking,
Apply/OK/Cancel, validation with task-dialog feedback, accessible controls, and
a versioned per-user registry schema. The host keeps committed settings separate
from page controls, so Cancel restores the committed values and a failed save
keeps the page dirty.

## Composition

1. `Settings` is the application-owned value object.
2. `LoadSettings` and `SaveSettings` borrow `HKEY_CURRENT_USER`, return structured
   status, and reject unknown schema versions, malformed types, and invalid data.
3. Each page creates its controls in `initialize` and owns a normal retained
   `Column` layout that grows with the native sheet.
4. Edit/click notifications call `SetDirty`; `validate` runs before `apply`.
5. `apply` builds a candidate value, persists it, and only then replaces the
   committed value. `reset` projects the committed value back to controls.
6. The model test uses a unique temporary registry key. The GUI self-test drives
   the real executable through both pages and verifies persisted presentation.

The GUI path also reads the explicit launcher/editor MSAA names and drives
runtime theme/settings refresh. Native property-sheet and dialog suites cover
keyboard traversal, DIP resize, cancellation, modal/modeless lifetime, and
bounded resource stress; the executable manifest is checked independently.

For a from-scratch walkthrough, follow
[`docs/tutorials/settings-application.md`](../tutorials/settings-application.md).
