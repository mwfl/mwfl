# Agent-oriented public API reference

This reference summarizes the contracts coding agents most often need. Exact
signatures live in the named public header and remain authoritative.

For machine-readable task routing, use `docs/api-index.json`. It connects each
common job to headers, public symbols, compiled examples, tests, and invariants.

## `RunApplication` and `Application`

- Header: `<mwtl/application.h>`
- Purpose: process entry, module/COM setup, main-window lifetime, message loop,
  and the outer exception boundary.
- Construction: normally call `RunApplication<MainWindow>(instance, show,
  options)` from `wWinMain`.
- Requirement: the window derives from the mwtl window marker, implements
  `void BuildUI()`, and is constructible from the supplied arguments.
- Failure: returns `EXIT_FAILURE` after reporting setup or uncaught run errors.
- Related: `ApplicationOptions`, `WindowOptions`, `MessagePump`.

## `WindowBase`

- Header: `<mwtl/window.h>`
- Ownership: owns its top-level HWND while attached; non-copyable.
- Threading: create, use, and destroy on the UI thread.
- Setup: override `BuildUI()`; store child controls as members.
- Events: override typed `OnCommand`, `OnClose`, `OnDpiChanged`, `OnWakeup`, or
  `OnMessage` handlers. Return `Propagate()` unless consuming the event.
- Common operations: `SetTitle`, `SetLayout`, `Close`, `GetHwnd`,
  `GetDpiContext`, `GetWakeup`, `SetAccelerators`.

## `ControlHost`

- Header: `<mwtl/control_host.h>`
- Purpose: checked creation of native child controls and automatic control-ID
  allocation during `BuildUI()`.
- Ownership: does not own the C++ control wrapper; controls remain window
  members.
- Use: `mwtl::ControlHost ui{*this}; ui.Add(button_, L"Save");`.
- Failure: creation failures use the library's checked diagnostic path.

## `NativeControl` and core controls

- Header: `<mwtl/controls.h>`
- Types: `Label`, `Button`, `TextBox`, `CheckBox`, `RadioButton`, `GroupBox`,
  `ComboBox`, `ListBox`, `ProgressBar`, and `Slider`.
- Ownership: a wrapper owns its child HWND; move-only; destruction is
  UI-thread-affine.
- Common operations: `GetHwnd`, `GetId`, `SetText`, `GetText`, `SetEnabled`,
  `SetVisible`, `Focus`, and `Destroy`.
- Creation: prefer `ControlHost`; direct `Create` is available when explicit
  IDs and bounds are required.
- Failure: most creating/mutating operations return `bool`; check them or use
  `Must` during setup.
- Typed choices: include `mwtl/selection.h` and use
  `SelectionAdapter<Control, Value>` with ComboBox, ListBox, or ComboBoxEx. It
  owns values outside Win32 item data, rejects invalid indices, and must be the
  exclusive item mutator on the control's UI thread.

## Layout

- Header: `<mwtl/layout.h>`
- Entry points: `Row()`, `Column()`, `Overlay()` return move-only `LayoutNode`.
- Sizing: `Auto(min,max)`, `Fixed(dip)`, `Stretch(weight,min,max)`.
- Units: `Dip`, usually written with `operator""_dip`.
- Lifetime: controls must have valid HWNDs when added and must outlive the
  retained layout.
- Behavior: layout repositions real child HWNDs on resize and DPI changes.

## Events and `EventResult`

- Header: `<mwtl/events.h>`
- Command matching: `event.IsClicked(control)` or `event.Is(control,
  notification_code)`.
- `Handled()`: the application consumed the event/result.
- `Propagate()`: continue default/native dispatch.
- Native escape: `WindowMessage` exposes message id, `WPARAM`, and `LPARAM`.

## Binding

- Header: `<mwtl/binding.h>`
- `ValueBinding<Control, Value>` keeps explicit references to a control and
  model value plus read/write/optional validation callables.
- `Pull()` validates before committing and returns a result with `accepted` and
  `message`; `Push()` writes the model to the control.
- Lifetime: binding, control, and model value must have compatible lifetimes.

## Split panes and Explorer composition

- Header: `<mwtl/splitter.h>`; canonical application: `examples/explorer`.
- Create both pane HWNDs as direct children of `Splitter`, then call
  `AttachPanes`. The splitter owns its container and borrows the two pane HWNDs.
- Attached-pane `WM_NOTIFY`, control `WM_COMMAND`, and `WM_CONTEXTMENU` are
  forwarded to the splitter parent, so route virtual ListView notifications in
  the ordinary top-level handlers.
- Keep the shared `VirtualListModel` application-authoritative. Use stable
  `TreeItemId`/`ListItemId`, `UpdateVirtualModel` for mutations that preserve
  selection, and `TakeVirtualException` after callback handling.
- Follow `docs/tutorials/explorer-application.md`; verify with
  `mwtl.explorer_model`, `mwtl.explorer_gui`, and `mwtl.splitter_native`.

## Property sheets and persisted Settings

- Header: `<mwtl/property_sheet.h>`; canonical application:
  `examples/property_sheet`.
- Give pages stable nonzero IDs. Create page controls in `initialize`, route
  native edit/click notifications through `command`, and call `SetDirty()`.
- `validate` runs before `apply`; return invalid and focus the offending control
  without mutating committed application state.
- Build a candidate from the committed value, persist it, then replace the
  committed value. Return `false` on persistence failure so dirty state remains.
- `reset` projects the committed value back to controls for Cancel. Modeless
  sheets and page callbacks stay on their creating UI thread.
- Follow `docs/tutorials/settings-application.md`; verify focused changes with
  `mwtl.settings_application_model` and `mwtl.settings_application_gui`.

## Commands

- Headers: `<mwtl/command.h>`, `<mwtl/command_controls.h>`, `<mwtl/desktop.h>`.
- `Command` contains a stable ID, label, callback, enabled/checked state, and
  optional shortcut.
- `CommandSet` stores commands and dispatches a `CommandEvent`.
- Native menu and toolbar presentations do not automatically redraw after all
  state changes; synchronize them as shown in `examples/commands`.
- A created `Menu` owns its handle until submenu transfer or window attachment.
  Use `TrackResult` for context menus so cancellation is not confused with
  native failure, then post its selected command through `WM_COMMAND`.

## Tooltips and image lists

- Header: `<mwtl/control_resources.h>`.
- `ImageList` owns its native list. `GetHandle()` and toolbar attachment are
  borrowed; the list must outlive every borrowing toolbar. Icon inputs are
  copied by the native list.
- `Tooltip` owns its popup HWND but borrows the owner and tool HWNDs. It owns
  copied text and supports update/removal, title, width, delay, activation, and
  event relay.
- Tooltip text is not an accessible name. Keep a visible label or call
  `SetAccessibleName` independently for an ambiguous control.

## TreeView and ListView data

- Header: `<mwtl/navigation_controls.h>`.
- Give tree and list rows stable nonzero `TreeItemId`/`ListItemId` values; do
  not put application object pointers in native item data.
- ListView is multi-select by default. Read `GetSelectedItemIds`; request
  `LVS_SINGLESEL` explicitly when needed.
- For owner data, create with `.virtual_data = true`, keep the authoritative
  data in a shared `VirtualListModel`, and route the control's `WM_NOTIFY` to
  `HandleNotification`. After handling, check `TakeVirtualException`.
- Use `UpdateVirtualModel` for reorder so stable selected IDs are restored;
  use `RefreshVirtualModel` for count/text-only refresh. Item insertion,
  subitem mutation, removal, state mutation, and native sorting are rejected.
- Sorting callbacks never cross Win32 with an exception. UI operations and the
  model remain on the creating thread; native HWNDs are borrowed.
- On a validated tree/list end-label notification, update the authoritative
  model and return `EventResult::Handled(TRUE)` to accept the native text.

## `WindowWakeup`

- Header: `<mwtl/wakeup.h>`
- Purpose: copyable, lifetime-safe worker-to-window notification token.
- Worker operation: `bool TryWake() const noexcept`.
- UI operation: override `OnWakeup()` and update controls there.
- Ownership: internally holds a weak reference; it does not keep the window
  alive. False during shutdown is expected.

## Timers and message pumps

- Headers: `<mwtl/timer.h>`, `<mwtl/message_pump.h>`.
- `UiTimer` is move-only RAII state associated with the window thread.
- The default pump handles ordinary applications. Use the wait-aware pump only
  when integrating kernel handles or explicit idle work.

## Desktop integration

- Headers: `<mwtl/desktop.h>`, `<mwtl/dialog.h>`.
- Includes menus, accelerator tables, file/folder dialogs, clipboard, file
  drops, and window placement.
- Dialog results distinguish acceptance, cancellation, and failure.
- `Dialog` is the move-only custom-content modal/modeless owner. Create child
  controls in `initialize`, then attach the normal retained layout. Its owner
  HWND is borrowed; `GetHwnd()` is a borrowed native escape hatch.
- Callbacks and child controls stay on the creating UI thread. Cross-thread
  `Accept`, `Cancel`, and destruction post work, so that thread must keep
  pumping messages until the close completes.
- Native handles returned or accepted by helpers retain the ownership stated by
  the corresponding API; never infer ownership from the raw handle type.

## Notification-area applications

- Header: `<mwtl/tray_icon.h>`.
- `TrayIcon` owns the shell registration but borrows its owner HWND and HICON.
- Use a stable application GUID, an `WM_APP` callback message, and a nonzero
  16-bit callback ID. Keep every mutation and destruction on the creating UI
  thread.
- In `OnMessage`, call `IsTaskbarCreated` and then `Recreate`; otherwise call
  `Decode` and handle the returned typed activation, context-menu, balloon, or
  popup event.
- Build context menus from the same `CommandSet` used by the main window so
  enabled/text state and dispatch remain authoritative.
- See `examples/hot_corners` and `docs/recipes/tray-icon.md`.

## Appearance, DPI, and accessibility

- Headers: `<mwtl/appearance.h>`, `<mwtl/dpi.h>`.
- `AppearanceOptions` requests system/light/dark mode, backdrop, and corners on
  a best-effort basis.
- High Contrast takes precedence over decorative choices.
- Layout uses DIPs. Raw Win32 APIs use pixels unless documented otherwise.
- Accessibility helpers set native names and dialog behavior; they supplement,
  not replace, correct labels and keyboard navigation.

## Checked operations

- Headers: `<mwtl/must.h>`, `<mwtl/error.h>`.
- `Must(value, context)` converts failed setup results into rich diagnostics.
- Use it for operations that must succeed before the window can function.
- Do not let resulting exceptions escape a Win32 callback.
