# Add a property page

Use `mwtl/property_sheet.h` when settings need standard Windows Apply, OK, and
Cancel behavior. The compiled canonical implementation is
`examples/property_sheet/main.cpp`.

1. Store the model independently from the controls.
2. Create move-only `PropertyPage` values with stable, nonzero IDs and titles.
3. Create child controls in `initialize`, then call `SetLayout` with the normal
   DIP-based row/column/overlay layout API.
4. Handle edit or click notifications in `command` and call that page's
   `SetDirty()`. Returning `true` means the command was consumed.
5. Return `invalid` from `validate` without mutating the committed model.
6. Copy valid control values into the model in `apply`; restore committed values
   in `reset`.
7. Pass the pages to `ShowModal`, or keep a `PropertySheetDialog` member and call
   `CreateModeless`.

The modeless owner and all page operations stay on the creating UI thread. The
sheet retains page callback state until it closes, but objects captured by those
callbacks must still outlive the sheet. Page HWNDs are borrowed and may be
created lazily; query `GetHwnd()` each time instead of caching it. A false return
from initialization/apply or a callback exception produces a failed structured
result. Cancellation calls `reset` and never implies that settings were saved.

For a modeless sheet inside `mwtl::Application`, keyboard dialog navigation is
registered automatically. The reference app demonstrates two pages, native
navigation, dirty/apply state, validation feedback, cancellation, responsive
layout, accessible names, and deterministic modeless ownership.
