# Current mwtl API

> This document describes the current public API. Stability guarantees and
> evolution rules are in [stability.md](stability.md).

## Commands and appearance

`DocumentState` is a UI-independent identity and dirty-state model for a single
document. Applications retain ownership of contents and file I/O. `MarkOpened`
and `MarkSavedAs` are called only after successful I/O; `EvaluateTransition`
returns `proceed`, `save_required`, or `cancelled` without displaying UI or
silently discarding changes. The object is not thread-safe and belongs to the
application's UI/document coordination thread.

`ReadTextFile` recognizes UTF-8 with or without a BOM and UTF-16 little- or
big-endian BOMs. It rejects malformed byte sequences instead of replacing data.
`WriteTextFileAtomic` writes and flushes a sibling temporary file before an
atomic replacement. Pass the `FileStamp` returned by the read operation to
reject a save when another process changed the destination. Results distinguish
missing paths, access failures, malformed encoding, external changes, and other
I/O failures without displaying UI.

`FindTextMatch`, `TextMatchesAt`, and `ReplaceAllText` provide UI-independent search
semantics for editor applications. Options cover direction, case matching, and
whole-word boundaries. Replacement advances beyond inserted text, so replacing
a token with text that contains the token cannot loop forever.

`TextHistory` owns a bounded sequence of text snapshots for deterministic Undo
and Redo in small document applications. It truncates the redo branch after a
new edit and remembers the saved entry; if that entry is evicted, the document
conservatively remains modified. Returned views remain valid only until the next
mutating history operation.

`RecentFileList` keeps a bounded, most-recent-first, case-insensitively
deduplicated path list. Registry persistence uses an explicit version-1 schema
with `SchemaVersion` (`REG_DWORD`) and `RecentFiles` (`REG_MULTI_SZ`). Load and
save results distinguish missing settings, access denial, malformed or unknown
schema data, and other I/O failures. The supplied root key is always borrowed.

`SingleInstance` uses a per-session named mutex for ownership and a registered
top-level HWND property plus bounded `WM_COPYDATA` for activation forwarding.
The primary explicitly registers and unregisters its window. A secondary waits
for startup only up to its caller-supplied timeout, and results distinguish no
receiver, timeout, access denial, and malformed or rejected delivery. Payloads
are copied and limited to 32,767 UTF-16 code units.

`Command` owns an application action's ID, display text, enabled, checked, and
visible state, optional toolbar image-list index, optional shortcut, and
callback. `CommandSet::Dispatch` can be returned directly from `OnCommand`.
`Menu::AppendCommand` and `Menu::UpdateCommand` project the same state into a
native menu without taking ownership of the command. Invisible commands are
skipped when a menu is built; hiding an existing item removes it, so showing it
again requires rebuilding that menu to preserve deliberate item ordering.
An attached `Menu` retains a non-owning handle for later updates while the
window owns the native menu. Reattaching a rebuilt menu releases the previous
native menu. `Toolbar::AddCommand` and `UpdateCommand` project all command state
onto toolbar buttons. `Toolbar::SetImageList` borrows an `ImageList`; that list
must outlive the toolbar and command image indices address its entries. Finally,
`AcceleratorTable::Create(CommandSet)` builds native shortcuts from all commands
that declare one.

Command image indices are non-negative positions in that borrowed list. A
command without an image uses `ClearImageIndex`; it does not own or destroy an
icon. Visibility affects presentation, not dispatch, so a deliberately hidden
command may remain reachable through its accelerator while enabled.

`WindowOptions::appearance` applies system/light/dark color preference, optional
Mica/Acrylic/Tabbed backdrops, and corner policy after HWND creation. Unsupported
DWM attributes are best-effort, and high-contrast mode always takes priority.
`SetAccessibleName` names a native control without visible text, while
`SetDialogDefaultButton` establishes keyboard default-button behavior.

## Lightweight value binding

`ValueBinding<Control, Value>` synchronizes an explicitly owned control and
model value through caller-provided read/write functions. `Pull` validates a
candidate before changing the model; `Push` uses a nested-safe `ChangeGate` so
programmatic updates can be ignored by change handlers. `BindText`,
`BindChecked`, and `BindSelection` cover common native controls. Bindings are
non-owning and non-copyable; both referenced objects must outlive them.

The API prioritizes concise, safe application code over historical source
compatibility.

## Concise ownership defaults

`ControlHost::Add(control, ...)` allocates a host-local control ID starting
at `0x4000`; pass a `ControlId` only when a stable resource ID is required.
`WindowBase::SetLayout(LayoutNode)` owns the layout tree, so there is no
non-owning layout attachment whose lifetime the caller must manage. Layout
measurement stores HWND values rather than references to movable wrappers.

Open, save, and folder pickers use the Vista-era `IFileDialog` family. Their
options use strings, booleans, and `std::filesystem::path` instead of legacy
hook callbacks and `OPENFILENAME`/`BROWSEINFO` flags.

## Layout and controls

All built-in controls expose dynamic intrinsic measurement. `Auto()` remeasures
whenever layout is measured, so text, content, font, theme, and DPI changes are
reflected without rebuilding the layout. Operations that may allocate can throw
instead of terminating through an invalid `noexcept` contract.

Native child controls remember their creating thread. Debug builds assert when
a control is used or destroyed from another thread. Use `WindowWakeup` for
cross-thread notification.

Public APIs do not use `[[nodiscard]]`; ordinary concise code needs no casts to
ignore native results.

## Startup dependency injection

`RunApplication` forwards constructor arguments to the main window:

```cpp
return mwtl::RunApplication<MainWindow>(
    instance, show, {.title = L"App"}, {}, settings, service);
```

The window does not need to be default-constructible.

The same constructor forwarding works with a custom message pump through the
consistent `Run(show, options, pump, arguments...)` member overload.

## Wait-aware message pump

The pump uses `std::chrono::milliseconds`, owns a copy of its handle list, and
accepts callbacks directly. There is no delegate base class to implement:

```cpp
using namespace std::chrono_literals;
mwtl::WaitAwareMessagePump pump({
    .handles = handles,
    .idle_interval = 16ms,
    .on_signal = [&](std::size_t index) { HandleSignal(index); },
    .on_idle = [&] { RenderIdleWork(); },
});
```

## Paths and file filters

Dialogs use `std::filesystem::path` and structured filters instead of exposing
the double-NUL Win32 filter encoding:

```cpp
auto selected = mwtl::ShowOpenFileDialog({
    .owner = GetHwnd(),
    .title = L"Open image",
    .filters = {
        {L"Images", L"*.png;*.jpg"},
        {L"All files", L"*.*"},
    },
});
```
