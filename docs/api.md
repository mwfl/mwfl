# Current mwfl API

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

Toolbars use the native list layout by default, keeping text beside an optional
image and vertically centering text-only buttons. Override `ToolbarOptions::style`
only when an application deliberately wants the legacy image-above-text layout.

`ImageList` exclusively owns the `HIMAGELIST` returned by `Create`; its
`GetHandle` result is borrowed and becomes invalid after move, `Reset`, or
destruction. Icons passed to add/replace are borrowed for the call because the
native list copies their pixels. Replace, remove, overlay, background-color,
count, and pixel-size operations read or mutate the native image list. Image
sizes are pixels at this Win32 boundary; an application chooses the appropriate
size when rebuilding resources for a new DPI. `Toolbar::SetImageList` borrows
the complete `ImageList`, which must outlive the toolbar.

`Tooltip` owns its tooltip HWND and borrows the owner plus every registered tool
HWND. Tools may be descendants of the owner and must be removed or destroyed
before the owner. Text is owned by `Tooltip`, so temporary strings are safe.
The API supports add/update/remove, active state, balloon title, maximum width,
delay times, explicit event relay, pop, and native refresh. Tooltips supplement
rather than replace visible labels and `SetAccessibleName`; assistive
technology does not depend on hover text.

`Menu` owns a newly created menu bar or popup until it transfers a submenu or
attaches a bar to a window. `MenuKind` and `IsOwned` expose that state, and
`GetHandle` is always borrowed. `PopupMenuResult` distinguishes a selected
command from user cancellation and validated setup failure. Invalid menu/owner
preconditions are `failed`; after valid tracking begins, native result zero is
`cancelled` because `TrackPopupMenuEx` provides no reliable extended error.
Command ID zero is rejected because native popup tracking reserves zero for no selection. Build menus from
`Command` to project text, enabled, checked, and visible state, then dispatch
the selected ID through the same `CommandSet`.

`StatusBar` is a native control with intrinsic measurement, so it can be placed
in a DIP layout with `Auto()` and remeasured during Per-Monitor-V2 DPI changes.
The Notepad reference application demonstrates that path, assigns MSAA names to
its text surface, toolbar, and status bar, and responds to theme/settings changes
using system fonts and colors. Its `mwfl.notepad_gui` test drives the real hidden
window through open, edit, atomic save, cancel, discard, reopen, DPI refresh, and
close without touching user settings.

`TabWorkspaceModel` gives application tabs stable nonzero `TabId` values while
keeping document objects and page HWND ownership in the application. It tracks
order, selection, title, dirty, and closable metadata. Removing the selected tab
selects its successor, or its predecessor when the removed tab was last.
Returned spans and pointers expire on the next model mutation; the model is
intended for the UI thread. `TabControl::Synchronize` projects the model into
native tab items and stores only numeric stable IDs in `TCITEM.lParam`.
`GetSelectedTabId` reads the native selection rather than returning cached
wrapper state. Synchronization returns `false` on a native insertion failure and
may leave a partial native projection; the caller retains the authoritative
model and can retry. The common-controls gallery is the canonical compact
example.

### Stable and virtual navigation data

`TreeView` stores only nonzero numeric `TreeItemId` values in native item
parameters. It can find, select, rename, remove, sort, label-edit, and set state
images by stable ID. `DecodeNotification` copies typed selection, edit, expand,
and state-change data while the notification is valid. Comparator exceptions
are captured inside the Win32 callback and rethrown only after native sorting
returns.

`ListView` uses the same contract through `ListItemId`. Normal report views
support multi-selection, stable-ID sorting, subitems, state images, label
editing, removal, and typed notification decoding. The default is multi-select;
add `LVS_SINGLESEL` explicitly when the product requires one selection.
When accepting an end-label notification, persist the validated text and return
`EventResult::Handled(TRUE)`; return zero or cancel the edit to reject it.

For large data, create with `ListViewOptions{.virtual_data = true}` and attach a
shared `VirtualListModel`. The application model remains authoritative and the
native owner-data control stores no application pointers. IDs must be unique
and nonzero. Route `WM_NOTIFY` to `HandleNotification`; then call
`TakeVirtualException` and rethrow on the ordinary mwfl event path if a text
callback failed. Mutating item APIs reject owner-data views: update the model
through `UpdateVirtualModel` to preserve selected stable IDs across reorder, or
call `RefreshVirtualModel` after a count/text-only change. All wrappers, models, sorting, and
notification routing stay on the creating UI thread; `GetHwnd()` remains a
borrowed native escape hatch.

### Typed selections

`SelectionAdapter<Control, Value>` binds application values to `ComboBox`,
`ListBox`, or `ComboBoxEx` labels without storing object pointers in native item
data. The adapter owns copied labels and values, validates every index, exposes
the selected value as an optional reference, and supports typed lookup,
removal, and clearing. It requires exclusive ownership of item mutations: keep
the adapter on the control's UI thread, destroy it before the control, and do
not add or reorder native items behind it. `IsSynchronized()` detects count
drift. Native handles remain borrowed escape hatches.

```cpp
mwfl::SelectionAdapter<mwfl::ComboBox, ThemeId> themes{theme_combo};
mwfl::Must(themes.Add(L"System", ThemeId::system), "add theme");
mwfl::Must(themes.Add(L"Dark", ThemeId::dark), "add theme");
mwfl::Must(themes.SelectValue(ThemeId::system), "select theme");
if (const auto selected = themes.GetSelectedValue()) SaveTheme(selected->get());
```

`Splitter` owns a focusable native child container and borrows two distinct
pane HWNDs created as its direct children. The application must detach panes
before destroying them independently; destroying the splitter naturally
destroys its child windows. Split positions, minimum sizes, bar thickness, and
keyboard steps are DIPs. Arrow keys move the bar, Home/End select its bounded
extremes, and mouse dragging emits `kSplitterPositionChanged` through
`WM_NOTIFY`. `Arrange` returns `false` with a Win32 last-error when the host or
borrowed panes are invalid; `SetPosition` and `SetConstraints` propagate that
result. `WM_NOTIFY`, control-originated `WM_COMMAND`, and `WM_CONTEXTMENU` from
either attached direct-child pane are synchronously forwarded to the
splitter's parent with their original parameters and result. This lets ordinary
top-level `OnNotify`, `OnCommand`, and context-menu routing work without
subclassing the panes. `SplitterModel` exposes the same constraint math
without HWNDs; when both minimums cannot fit, it divides the available space
proportionally and reports `constraints_satisfied == false`. The
Explorer reference application is the complete native composition example.

`PropertyPage` owns the C++ state and callbacks for one native page; the native
page HWND exists only while a sheet has created it. `PropertySheetDialog`
retains page state even if the caller's page wrappers leave scope. Its modeless
form is a move-only HWND owner and integrates `PropSheet_IsDialogMessage` with
an initialized mwfl message loop. All operations belong to the creating UI
thread. Page callbacks cover initialization, commands, validation, apply, and
reset; exceptions are captured in `PropertySheetResult` and never cross the
Win32 callback boundary. `SetDirty` controls the native Apply button, and a
failed validation or apply keeps the page dirty. Resizable sheets use the normal
`LayoutHost` page layout and keep tabs, pages, and buttons anchored. See the
property-sheet reference application and `docs/recipes/property-page.md`.

`ShowTaskDialog(TaskDialogOptions)` exposes native common/custom buttons, radio
buttons, verification state, expandable/footer text, flags, width, and a
structured `TaskDialogResult`. Its callback maps created, navigation, button,
radio, verification, hyperlink, timer, help, expando, and destruction events.
The callback-scoped `TaskDialogController` can enable or click choices, update
text, and drive determinate or marquee progress. Returning `keep_open` from a
button event rejects that close attempt. Callback exceptions are captured in
the result and trigger safe cancellation; no exception crosses comctl32. The
owner HWND is borrowed for the synchronous modal call, and callback/controller
operations stay on that UI thread. `IsTaskDialogAvailable` checks the exported
native function; failures remain HRESULTs. The original five-argument overload
remains the compact message-dialog path.

`Dialog` is the general custom-content counterpart. It owns its native HWND
while modeless, uses the native `DialogBoxIndirectParamW` nested loop while
modal, and returns `DialogResult` with distinct accepted, cancelled, and failed
states. `DialogOptions::owner` is borrowed. Native modal display disables the
owner; a modeless dialog leaves it enabled unless
`disable_owner_for_modeless` requests a pseudo-modal workflow, in which case
the owner is restored exactly once during every close path. Controls are
created in `DialogCallbacks::initialize`, and `SetLayout` accepts the same
retained `Row`/`Column`/`Overlay` tree used by `WindowBase`.

All dialog operations belong to the creating UI thread. `Accept` and `Cancel`
may be requested from another thread and are posted to that thread, but callers
must not otherwise access the dialog or child controls cross-thread. Callback
exceptions are captured in `DialogResult::callback_exception`, converted to a
failed result, and never cross the Win32 callback boundary. A user `WM_CLOSE`
may be vetoed by the close callback; programmatic `Close` is deterministic and
cannot be vetoed. The move-only wrapper prevents duplicate HWND ownership, and
destruction closes an active modeless dialog. See
`examples/desktop_integration` for the canonical modal form.

`TrayIcon` is a move-only owner for one Windows notification-area
registration. `TrayIconOptions` requires a stable application-defined GUID and
a nonzero 16-bit callback ID. The owner HWND, callback message, and HICON are
borrowed; the owner and icon must outlive the registration. `Add` selects
`NOTIFYICON_VERSION_4`, after which `Decode(WindowMessage)` maps native mouse,
keyboard, context-menu, balloon, and popup messages to `TrayIconEvent` with
screen coordinates. Tooltip, icon, visibility, and balloon-notification updates
change cached state only after `Shell_NotifyIconW` succeeds.
`TrayIconStateModel` is the HWND-free state machine used by the wrapper; it
distinguishes initial add, retryable Explorer recovery, successful
registration, and deterministic detach. Applications normally inspect
`TrayIcon::GetState`; the model is public for deterministic orchestration and
unit testing without depending on the notification area.

Explorer does not retain notification registrations across a restart. Route
the registered message recognized by `IsTaskbarCreated` to `Recreate`; a failed
retry remains `TrayIconState::recovery_pending` instead of pretending the icon
exists. `Remove` and destruction are idempotent and abandon local identity even
if an already-destroyed owner makes the shell deletion fail. All mutation and
destruction belong to the creating UI thread. The GUID and callback ID are
application identity, not owned resources. Hot Corners is the canonical
Windows 10+ tray utility; `mwfl.tray_icon_state` proves every state transition
without HWNDs and `mwfl.tray_icon_native` exercises the real shell
protocol.

`WindowOptions::appearance` applies system/light/dark color preference, optional
Mica/Acrylic/Tabbed backdrops, and corner policy after HWND creation. Unsupported
DWM attributes are best-effort, and high-contrast mode always takes priority.
`WindowBase` retains that option and automatically reapplies it on
`WM_SETTINGCHANGE`, `WM_THEMECHANGED`, and `WM_SYSCOLORCHANGE`. Reapplication
updates the DWM frame, attached menu, client-area colors, and Explorer-compatible
visual styles throughout the native child HWND tree. Controls created later by
MWFL inherit the effective state from their parent.

`GetAppearanceState()` exposes the window's current `AppearanceState`.
`OnAppearanceChanged(const AppearanceState&)` runs after automatic reapplication;
custom GDI drawing should rebuild borrowed/owned brushes there from the supplied
palette. `D2DRenderContext::appearance` supplies the same effective palette to
Direct2D paint callbacks. `SetAppearance` changes the retained preference and
applies it immediately. These calls belong to the creating UI thread. Native
theme APIs remain best-effort: Windows and third-party controls may paint portions
themselves, and High Contrast always replaces requested light/dark colors.
`ApplyWindowAppearanceBestEffort` is the lower-level alternative for a borrowed
raw HWND; its boolean result reports that MWFL accepted the request, not that
every requested DWM attribute took effect. It does not retain a policy for later
theme notifications.
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

## Optional Direct2D host

`<mwfl/d2d_host.h>` is supplied by the optional `mwfl::d2d` target. The core
`mwfl::ui` target does not link Direct2D. Installed consumers use
`find_package(mwfl CONFIG REQUIRED COMPONENTS d2d)` and link `mwfl::d2d`.

`D2DHost` owns its child HWND, factory, HWND render target, and every
`BeginDraw`/`EndDraw` transaction. `D2DHostCallbacks::paint` receives a borrowed
`D2DRenderContext`; it must not retain the target/context or begin/end drawing.
Device-dependent brushes belong in `create_device_resources` and are released
in `discard_device_resources`. `D2DERR_RECREATE_TARGET` discards the target and
the next render recreates it. Callback exceptions become `E_UNEXPECTED` and are
retrieved with `TakeCallbackException`.

Sizes and input positions are DIPs. A zero-area/minimized host is a successful
no-op. Keep document/stroke/image data application-owned so GPU resource loss
never loses user content. See `examples/drawing` and
`docs/recipes/direct2d-drawing.md`.

## Optional Direct3D swap-chain host

`<mwfl/d3d_host.h>` belongs to the optional `mwfl::d3d` target. Installed
consumers request `COMPONENTS d3d`; the core target exposes no `d3d11` or
`dxgi` linkage. `D3DHost` owns its child HWND, D3D11 device/context, flip-model
BGRA8 swap chain, and render-target view. `RenderFrame` renders one requested
frame and never creates a hidden game loop.

`D3DFrameResult` distinguishes presented, minimized, occluded, recreated, and
failed outcomes. Hardware creation may fall back to WARP only when enabled;
query `UsesSoftwareAdapter`. Resize during a render callback is deferred until
the frame ends. Device removal discards callback-owned device resources and
recreates the device/swap chain. Callback views are borrowed, UI-thread-only,
and invalid after return or teardown. See `docs/recipes/direct3d-swap-chain.md`.

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
Default intrinsic sizes use compact Windows desktop metrics: text and combo
inputs have a 96-DIP minimum width, collection controls 144 DIPs, and compact
single-line controls target a 22-DIP height. Applications can always override
these defaults with `Fixed`, minimum/maximum constraints, or `preferred_size`.

Native child controls remember their creating thread. Debug builds assert when
a control is used or destroyed from another thread. Use `WindowWakeup` for
cross-thread notification.

`TextBox` exposes the ordinary native editing operations without requiring an
application to send edit-control messages: selection read/write,
selection replacement, Cut, Copy, Paste, Undo, `CanUndo`, and caret scrolling.
`SetCueBanner` supplies native placeholder guidance without becoming the
control value or an accessible replacement for its explicit name.
`TextSelection` uses zero-based UTF-16 code-unit offsets, matching the native
EDIT contract. An invalid HWND or a range beyond native `LONG` limits makes
`SetSelection` fail; clipboard commands remain best-effort native operations.
The control owns its HWND, while clipboard contents and returned selection
values are copied or process-global native state.

Public APIs do not use `[[nodiscard]]`; ordinary concise code needs no casts to
ignore native results.

## Startup dependency injection

`RunApplication` forwards constructor arguments to the main window:

```cpp
return mwfl::RunApplication<MainWindow>(
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
mwfl::WaitAwareMessagePump pump({
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
auto selected = mwfl::ShowOpenFileDialog({
    .owner = GetHwnd(),
    .title = L"Open image",
    .filters = {
        {L"Images", L"*.png;*.jpg"},
        {L"All files", L"*.*"},
    },
});
```

## Optional WIC imaging

`<mwfl/imaging.h>` belongs to `mwfl::imaging`. `DecodeImageFile` requires COM
on the caller thread and synchronously returns a structured status plus
application-owned, top-down premultiplied BGRA8 pixels. Dimensions and the
default 256 MiB decoded-pixel budget are checked before allocation. Embedded
profile presence and successful conversion to sRGB are separate flags; invalid
profiles fall back without claiming color management.

`ImageViewportModel` is HWND/GPU-free. Image units are pixels; viewport, pan,
and output origin are DIPs. It implements bounded Fit, anchored zoom, and
clamped pan. Keep `DecodedImage` authoritative and treat a D2D bitmap as a
disposable device cache. See `examples/image_viewer` and
`docs/tutorials/image-viewer.md`.

## Printing, OLE, and Shell components

`<mwfl/printing.h>`, `<mwfl/printing_native.h>`, and
`<mwfl/printing_settings.h>` belong to `mwfl::printing`. Pagination and preview
models are independent of HWNDs and printers. `PrintJob` owns the native
StartDoc/StartPage transaction and aborts incomplete work; render callbacks
borrow the HDC. `PrintPages` accepts a pre-page cancellation check and contains
exceptions from both callbacks. Printer enumeration, capabilities, DEVMODE/DEVNAMES settings,
dialog cancellation, and native failures have structured results.

`<mwfl/ole_data.h>` and `<mwfl/ole_drag_drop.h>` belong to `mwfl::ole` and
require `ComApartment::ole_sta` for drag/drop. Data objects copy and bound
Unicode, file-list, custom, and delayed payloads. COM reference counts and
STGMEDIUM transfer determine ownership. Drop callbacks are reentrant UI-thread
calls; revoke registrations before destroying the target HWND.

`<mwfl/settings_store.h>`, `<mwfl/file_association.h>`, and
`<mwfl/shell_integration.h>` belong to `mwfl::shell`. Settings use an explicit
schema version written last. Associations are per-user, owner-marked, and
reversible without removing foreign registry state. Jump Lists use stable task
IDs and an STA transaction. `TaskbarWindowIntegration` is creating-thread-only;
clear it on teardown and recreate/reapply after `TaskbarCreated`. Explorer,
policy, access, and COM failures remain visible structured outcomes.

See `examples/printing`, `examples/ole_drag_drop`,
`examples/shell_integration`, and
`docs/tutorials/printing-ole-shell.md`.
## Generic native host

`<mwfl/native_host.h>` provides `NativeHost`, an owned container HWND that
borrows one same-thread, same-process direct-child HWND. `Attach` arranges the
child and enables focus plus notification forwarding; `Detach` stops management
without destroying or reparenting. Parent destruction follows normal Win32
child destruction. `NativeHostStateModel` exposes the HWND-free state machine.

Forwarded `WM_NOTIFY`, control `WM_COMMAND`, and `WM_CONTEXTMENU` data remain
message-scoped borrowed memory. All host operations are creating-thread-only.
Use a specialized host when the integration does not create an attachable HWND.

## Optional WebView2 host

`<mwfl/webview2.h>` is supplied by `mwfl::webview2`, enabled with
`MWFL_BUILD_WEBVIEW2`. The optional component pins official SDK
`1.0.4129.50`; core `mwfl::ui` does not fetch or link it. Installed consumers
request `COMPONENTS webview2`.

`QueryWebView2Runtime` reports `available`, `missing`, or `failed` and never
installs software or displays UI. `WebView2Host::Initialize` asynchronously
creates an environment and controller on the creating STA. Its structured
result distinguishes host state, runtime state, and HRESULT. Initialization,
navigation, process-failure, and accelerator callbacks contain exceptions.

The host owns its container, environment, controller, web view, and event
subscriptions. `GetController` and `GetWebView` are borrowed SDK escape hatches.
`Close` invalidates pending callbacks, removes subscriptions, and closes the
controller exactly once. `Restart` recreates the environment/controller after
a process failure; post a window message before calling it from an event
callback. Resize and parent DPI messages update controller bounds, and host
focus moves into web content. Tests use `NavigateToString` to remain offline.

## Optional Scintilla editor

`<mwfl/scintilla.h>` is supplied by `mwfl::scintilla`, enabled with
`MWFL_BUILD_SCINTILLA`. It pins Scintilla 5.6.5 source and x64 runtime archives.
Installed consumers request `COMPONENTS scintilla`, link the target, and invoke
`mwfl_deploy_scintilla(target)`.

`ScintillaRuntime` returns structured missing/wrong-architecture/failure state.
Its loaded module state is shared with every created editor so a live HWND never
outlives its code. `ScintillaEditor` owns its HWND and default native document;
raw document or loader handles returned through `Send` follow the pinned native
ownership contract.

Public strings convert strictly between UTF-16 and UTF-8. All positions are
UTF-8 byte offsets. Typed notifications cover modification, save points,
characters, and UI updates. High-level operations include code-style setup,
line-number margins, selection, find/replace, undo/redo, clipboard commands,
read-only state, dirty state, and zoom. HWND operations and notifications stay
on the creating UI thread.

## Multi-document workspace

`<mwfl/document_workspace.h>`, `<mwfl/document_coordination.h>`,
`<mwfl/document_tabs.h>`, and `<mwfl/document_session.h>` are core
`mwfl::ui` surfaces for Windows 10+ C++20 applications. Installed,
`add_subdirectory`, and `FetchContent` consumers acquire no rendering, OLE,
Shell, or mandatory Document/View dependency.

`DocumentWorkspaceModel` is a UI-thread-coordinated logical model. It owns
metadata snapshots only: stable IDs, title/path, order, active ID, dirty/undo
projection, view state, and bounded recently closed metadata. Applications own
contents, undo buffers, file stamps, document types, and optional view objects
keyed by `DocumentId`; no base class is required. Returned spans and pointers
expire at the next mutation. There is no global active document.

`RouteActiveDocument` snapshots an ID, contains callback exceptions, and
reports reentrant activation. Command projection updates only text/enabled;
application-owned checked state, shortcut, visibility, image, and handler stay
intact. `status_text` is suitable for a status bar. Callbacks run without model
or persistence locks.

`DocumentTabWorkspaceAdapter` is creating-UI-thread-only and borrows its
`TabControl` and page HWNDs. Pages share the TabControl parent so notifications
reach the workspace. Unbind/detach before application-owned page destruction.
Native items contain integer `DocumentId` values, never pointers.
`TransferDocumentWithPage` adopts destination metadata before reparenting and
source removal; failure restores metadata and HWND parentage. A
`projection_failure` leaves logical ownership valid for a native rebuild.

`DocumentSession` stores logical metadata, optional `FileStamp`, and view state,
not contents. Its schema is versioned, bounded by UTF-8 bytes, strict for known
records, tolerant of tagged extensions, and atomically replaced. Applications
classify absolute/trusted, missing, changed, or rejected paths and restore
contents through callbacks. Malformed, duplicate, truncated, oversized, or
unsupported state never mutates a workspace. Restore callbacks must not mutate
the target; revision changes reject the item and exceptions are captured.

Coordinated close is inspect/decide/commit. Collect every decision before
destroying a window. Synchronous callers use `ExecuteCoordinatedClose`;
asynchronous callers complete every `save_before_close` ID and then call
`CommitCoordinatedCloseAfterSaves`. Stale revisions, failed saves, exceptions,
and cancellation preserve ownership. `WindowOptions::quit_on_destroy = false`
is for application-coordinated auxiliary top-level windows.

See `examples/document_workspace`, `docs/tutorials/document-workspace.md`, and
`docs/recipes/multi-document-workspace.md`.

## Docking workspace

`<mwfl/docking_workspace.h>` owns the stable-ID logical graph and explicit
proposal/commit transaction. `<mwfl/docking_native.h>` projects snapshots into
application-owned panel and group HWNDs with prepare/adopt/rollback/commit.
`<mwfl/docking_drag.h>`, `<mwfl/docking_preview.h>`, and
`<mwfl/docking_keyboard.h>` separate input, target selection, non-destructive
feedback, and acceptance. `<mwfl/docking_floating.h>`,
`<mwfl/docking_auto_hide.h>`, and `<mwfl/docking_monitor.h>` define auxiliary
host and monitor policy. `<mwfl/docking_session.h>` owns versioned persistence.
All are core `mwfl::ui` surfaces for Windows 10+ C++20 applications.

`DockLayoutModel` owns metadata copies only. `DockPanelId`, `DockGroupId`,
`DockNodeId`, and `DockFloatingHostId` are stable nonzero application IDs;
they are never HWNDs or pointers. Applications own content, commands,
documents, and HWND destruction. Proposals do not mutate the model. Prepare and
adopt native moves first, commit the logical transaction second, then commit
the adoption; rollback restores parent, styles, visibility, and focus after a
failure or stale revision.

Native adapters and hosts are creating-UI-thread-only. They validate process,
thread, parentage, and stale-window boundaries and borrow panel/group HWNDs.
`DockFloatingWindow` owns its auxiliary top-level HWND, borrows one content
host, applies DPI/minimum-size policy, restores content during teardown, and
does not post process quit. `DockPreviewWindow` is nonactivating and
input-transparent. Callback exceptions are contained at every native boundary.

`DockDragSession` retains logical snapshots only. Hit testing consumes bounded
screen-space DIP targets; pointer capture, preview HWNDs, and adoption remain
application/native responsibilities. Escape, capture loss, destruction,
invalid proposals, rejected adoption, stale transactions, reentrancy, and
exceptions cancel without half-committed state. `DockKeyboardSession` exposes
the same targets through arrows, Tab/Shift+Tab, Enter, Escape, and accessible
announcements.

Docking sessions contain only logical IDs, ratios, edges, bounded text, and DIP
placement. The parser is versioned and bounded, accepts unknown optional tagged
fields, and rejects malformed, duplicate, truncated, oversized, cyclic, or
unsupported graphs. Atomic file save replaces only a completely written
candidate. Recover floating placement against injectable current monitor work
areas before native adoption and use a deterministic safe default on failure.

See `examples/docking_workspace`,
`docs/tutorials/docking-workspace.md`, and
`docs/recipes/docking-workspace.md`.

## Optional Windows Ribbon

`<mwfl/ribbon.h>` is supplied by the `ribbon` package component and target
`mwfl::ribbon`; it is not part of the umbrella header. `RibbonCommandModel`
owns stable command bindings, application-mode masks, contextual visibility,
and copied bounded recent-item metadata. It contains no HWND, COM interface, or
application pointer. `Project` maps ordinary `CommandSet` presentation state,
and `Execute` invokes the current command while containing exceptions.

`RibbonFrameworkHost` borrows the owner frame, model, and `CommandSet`, owns
`IUIFramework` and application callbacks, and remains on its creating STA UI
thread. The executable owns its UICC-generated resource. `Load` consumes that
resource; `SetModes` and invalidation update native state. `GetFramework` is a
borrowed escape hatch. Framework absence is a structured supported fallback;
broken markup/resource loading is a packaging failure. Destroy the host before
the frame and COM apartment. See `examples/ribbon_workspace` and
`docs/tutorials/ribbon.md`.

## Optional legacy MDI

`<mwfl/mdi.h>` is the optional `mdi` component. Prefer the modern document
workspace for new products. `MdiWorkspaceModel` owns pointer-free stable child
IDs, titles, order, active selection, dirty state, and close policy; application
documents and views remain external. Move and destination-first transfer are
bounded and revisioned. `RouteMdiActiveChild` snapshots a stable ID, contains
exceptions, and reports reentrant activation.

`MdiHost` owns one MDICLIENT and its native children while borrowing the frame,
window menu, model, and message callbacks. It supports activate, close, cascade,
horizontal/vertical tile, icon arrangement, accelerator translation, and frame
default processing. HWNDs returned by `GetClient` and `GetChild` are borrowed.
Collect every dirty save/discard/cancel decision before closing any child; a
cancel or failed save must leave the native set intact. Stop callbacks and
destroy on the creating UI thread. See `examples/mdi_workspace` and
`docs/tutorials/mdi.md`.

## Optional enhanced metafiles and GDI+

`<mwfl/graphics.h>` is the optional `graphics` component. `EnhancedMetafile`
exclusively owns one HENHMETAFILE and is move-only; `Release` transfers the
`DeleteEnhMetaFile` obligation. `RecordEnhancedMetafile` lends its recording HDC
only during the callback, always closes it, deletes partial output after an
exception, and treats an explicit frame as 0.01 millimeter units. Playback
destinations are device pixels. Load, save, play, and every invalid rectangle
return structured status.

`GdiPlusSession` explicitly pairs startup/shutdown on its creating thread.
`ExportGdiPlusPng` bounds dimensions to 32768 and pixels to 67,108,864, lends a
`Gdiplus::Graphics` only during the callback, contains exceptions, locates the
PNG encoder, and replaces through a sibling temporary file. See
`examples/graphics_interop` and `docs/tutorials/graphics-help.md`.

## Taskbar commands and contextual Help

The optional `shell` component extends `TaskbarWindowIntegration` with stable-ID
thumbnail buttons and registered taskbar tabs in addition to progress and
overlay state. Icons and HWNDs are borrowed for calls; COM objects and mutation
remain on the creating STA UI thread. After `TaskbarCreated`, call `Recreate`
and reapply all application-owned state. Clear reversible state before window
teardown.

`<mwfl/help.h>` accepts absolute existing local CHM/HTML or an HTTPS URI. It
rejects UNC paths, traversal, embedded controls, quotes, credentials, whitespace,
wrong extensions, and non-HTTPS network schemes. `LaunchHelp` calls HtmlHelpW or
ShellExecuteExW without composing a command line. Missing, unavailable,
cancelled, native, and callback failures are distinct. Use
`LaunchHelpWithBackend` for deterministic offline tests. See
`examples/shell_integration` and `docs/tutorials/graphics-help.md`.
