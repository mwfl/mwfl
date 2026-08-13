# Agent-oriented public API reference

This reference summarizes the contracts coding agents most often need. Exact
signatures live in the named public header and remain authoritative.

For machine-readable task routing, use `docs/api-index.json`. It connects each
common job to headers, public symbols, compiled examples, tests, and invariants.

## `RunApplication` and `Application`

- Header: `<mwfl/application.h>`
- Purpose: process entry, module/COM setup, main-window lifetime, message loop,
  and the outer exception boundary.
- Construction: normally call `RunApplication<MainWindow>(instance, show,
  options)` from `wWinMain`.
- Requirement: the window derives from the mwfl window marker, implements
  `void BuildUI()`, and is constructible from the supplied arguments.
- Failure: returns `EXIT_FAILURE` after reporting setup or uncaught run errors.
- Related: `ApplicationOptions`, `WindowOptions`, `MessagePump`.

## `WindowBase`

- Header: `<mwfl/window.h>`
- Ownership: owns its top-level HWND while attached; non-copyable.
- Threading: create, use, and destroy on the UI thread.
- Setup: override `BuildUI()`; store child controls as members.
- Events: override typed `OnCommand`, `OnClose`, `OnDpiChanged`, `OnWakeup`, or
  `OnMessage` handlers. Return `Propagate()` unless consuming the event.
- Common operations: `SetTitle`, `SetLayout`, `Close`, `GetHwnd`,
  `GetDpiContext`, `GetWakeup`, `SetAccelerators`.

## `ControlHost`

- Header: `<mwfl/control_host.h>`
- Purpose: checked creation of native child controls and automatic control-ID
  allocation during `BuildUI()`.
- Ownership: does not own the C++ control wrapper; controls remain window
  members.
- Use: `mwfl::ControlHost ui{*this}; ui.Add(button_, L"Save");`.
- Failure: creation failures use the library's checked diagnostic path.

## `NativeControl` and core controls

- Header: `<mwfl/controls.h>`
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
- Typed choices: include `mwfl/selection.h` and use
  `SelectionAdapter<Control, Value>` with ComboBox, ListBox, or ComboBoxEx. It
  owns values outside Win32 item data, rejects invalid indices, and must be the
  exclusive item mutator on the control's UI thread.

## Layout

- Header: `<mwfl/layout.h>`
- Entry points: `Row()`, `Column()`, `Overlay()` return move-only `LayoutNode`.
- Sizing: `Auto(min,max)`, `Fixed(dip)`, `Stretch(weight,min,max)`.
- Units: `Dip`, usually written with `operator""_dip`.
- Lifetime: controls must have valid HWNDs when added and must outlive the
  retained layout.
- Behavior: layout repositions real child HWNDs on resize and DPI changes.

## Events and `EventResult`

- Header: `<mwfl/events.h>`
- Command matching: `event.IsClicked(control)` or `event.Is(control,
  notification_code)`.
- `Handled()`: the application consumed the event/result.
- `Propagate()`: continue default/native dispatch.
- Native escape: `WindowMessage` exposes message id, `WPARAM`, and `LPARAM`.

## Binding

- Header: `<mwfl/binding.h>`
- `ValueBinding<Control, Value>` keeps explicit references to a control and
  model value plus read/write/optional validation callables.
- `Pull()` validates before committing and returns a result with `accepted` and
  `message`; `Push()` writes the model to the control.
- Lifetime: binding, control, and model value must have compatible lifetimes.

## Split panes and Explorer composition

- Header: `<mwfl/splitter.h>`; canonical application: `examples/explorer`.
- Create both pane HWNDs as direct children of `Splitter`, then call
  `AttachPanes`. The splitter owns its container and borrows the two pane HWNDs.
- Attached-pane `WM_NOTIFY`, control `WM_COMMAND`, and `WM_CONTEXTMENU` are
  forwarded to the splitter parent, so route virtual ListView notifications in
  the ordinary top-level handlers.
- Keep the shared `VirtualListModel` application-authoritative. Use stable
  `TreeItemId`/`ListItemId`, `UpdateVirtualModel` for mutations that preserve
  selection, and `TakeVirtualException` after callback handling.
- Follow `docs/tutorials/explorer-application.md`; verify with
  `mwfl.explorer_model`, `mwfl.explorer_gui`, and `mwfl.splitter_native`.

## Property sheets and persisted Settings

- Header: `<mwfl/property_sheet.h>`; canonical application:
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
  `mwfl.settings_application_model` and `mwfl.settings_application_gui`.

## Commands

- Headers: `<mwfl/command.h>`, `<mwfl/command_controls.h>`, `<mwfl/desktop.h>`.
- `Command` contains a stable ID, label, callback, enabled/checked state, and
  optional shortcut.
- `CommandSet` stores commands and dispatches a `CommandEvent`.
- Native menu and toolbar presentations do not automatically redraw after all
  state changes; synchronize them as shown in `examples/commands`.
- A created `Menu` owns its handle until submenu transfer or window attachment.
  Use `TrackResult` for context menus so cancellation is not confused with
  native failure, then post its selected command through `WM_COMMAND`.

## Tooltips and image lists

- Header: `<mwfl/control_resources.h>`.
- `ImageList` owns its native list. `GetHandle()` and toolbar attachment are
  borrowed; the list must outlive every borrowing toolbar. Icon inputs are
  copied by the native list.
- `Tooltip` owns its popup HWND but borrows the owner and tool HWNDs. It owns
  copied text and supports update/removal, title, width, delay, activation, and
  event relay.
- Tooltip text is not an accessible name. Keep a visible label or call
  `SetAccessibleName` independently for an ambiguous control.

## TreeView and ListView data

- Header: `<mwfl/navigation_controls.h>`.
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

- Header: `<mwfl/wakeup.h>`
- Purpose: copyable, lifetime-safe worker-to-window notification token.
- Worker operation: `bool TryWake() const noexcept`.
- UI operation: override `OnWakeup()` and update controls there.
- Ownership: internally holds a weak reference; it does not keep the window
  alive. False during shutdown is expected.

## Timers and message pumps

- Headers: `<mwfl/timer.h>`, `<mwfl/message_pump.h>`.
- `UiTimer` is move-only RAII state associated with the window thread.
- The default pump handles ordinary applications. Use the wait-aware pump only
  when integrating kernel handles or explicit idle work.

## Desktop integration

- Headers: `<mwfl/desktop.h>`, `<mwfl/dialog.h>`.
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

- Header: `<mwfl/tray_icon.h>`.
- `TrayIcon` owns the shell registration but borrows its owner HWND and HICON.
- `TrayIconStateModel` is the pure state machine behind initial add, retryable
  Explorer recovery, successful registration, and detach; use it when business
  logic must be tested without a live notification area.
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

- Headers: `<mwfl/appearance.h>`, `<mwfl/dpi.h>`.
- `AppearanceOptions` requests system/light/dark mode, backdrop, and corners on
  a best-effort basis.
- `WindowBase` retains the option and automatically reapplies its frame, native
  descendants, attached menu, and palette after Windows theme/settings changes.
- Use `GetAppearanceState()` or `OnAppearanceChanged` for GDI/custom resources;
  Direct2D paint callbacks receive the same state as `D2DRenderContext::appearance`.
- High Contrast takes precedence over decorative choices.
- Layout uses DIPs. Raw Win32 APIs use pixels unless documented otherwise.
- Accessibility helpers set native names and dialog behavior; they supplement,
  not replace, correct labels and keyboard navigation.

## Checked operations

- Headers: `<mwfl/must.h>`, `<mwfl/error.h>`.
- `Must(value, context)` converts a failed checked value into a setup diagnostic.
- Prefer `MustInvoke(operation, context)` when a Win32 call's `GetLastError`
  value should be preserved; an already-evaluated `bool` cannot retain it.
- Use it for operations that must succeed before the window can function.
- Do not let resulting exceptions escape a Win32 callback.

## WIC image viewing

- Request `COMPONENTS imaging d2d`; link `mwfl::imaging` and `mwfl::d2d`.
- Initialize COM (normally `ApplicationOptions{.com_apartment = sta}`) before
  `DecodeImageFile`. Handle structured missing, unsupported, invalid, too-large,
  COM, memory, and generic failure statuses.
- Keep `DecodedImage` CPU pixels authoritative. Recreate `ID2D1Bitmap` inside
  the D2D device-resource callback after target loss or a new image.
- Image coordinates are pixels; `ImageViewportModel` outputs DIP geometry.
- For `WM_MOUSEWHEEL`, use the DIP client point supplied by `D2DInputEvent`;
  raw Win32 wheel coordinates are screen coordinates.
- Follow `docs/tutorials/image-viewer.md` and `examples/image_viewer`.

## Direct2D drawing

- Header: `<mwfl/d2d_host.h>`; CMake target: `mwfl::d2d`.
- For installed packages request `COMPONENTS d2d`. Do not add Direct2D to the
  core target or include the optional header when it is not requested.
- Store `D2DHost` and device resources as window members. Create brushes in
  `create_device_resources`, release them in `discard_device_resources`, and
  draw only inside `paint`.
- Never call `BeginDraw`/`EndDraw` or retain `D2DRenderContext`/its target.
- Keep application content GPU-independent and use DIP coordinates.
- Layout has no `Spacer()` helper. Use only `Row`, `Column`, `Margin`, `Gap`,
  `Auto`, `Fixed`, and `Stretch` APIs present in `<mwfl/layout.h>`.
- Start from `examples/drawing` and `docs/recipes/direct2d-drawing.md`.

## Direct3D swap chain

- Header: `<mwfl/d3d_host.h>`; CMake target: `mwfl::d3d`; installed packages
  require `COMPONENTS d3d`.
- Call `RenderFrame` when the application needs a frame. Do not add an implicit
  timer or game loop unless the application explicitly requires one.
- Handle every `D3DFrameStatus`: minimized is a successful no-op, occluded
  should be retried only after later activity, and `device_recreated` means the
  requested frame was not presented.
- Device/context/swap-chain/RTV pointers are borrowed escape hatches. Put GPU
  resources in create/discard callbacks and keep authoritative data outside.
- `allow_warp_fallback` controls software rendering; never silently claim
  hardware acceleration. See `docs/recipes/direct3d-swap-chain.md`.
## Optional integration selection

- Generic child HWND: include `mwfl/native_host.h`, link `mwfl::ui`, create
  the third-party child with the host as parent, then `Attach` it.
- Web content: set `MWFL_BUILD_WEBVIEW2=ON`, include `mwfl/webview2.h`, and link
  `mwfl::webview2`. Run in an STA, wait for ready, handle missing Runtime, and
  use `NavigateToString` in tests.
- Source editor: set `MWFL_BUILD_SCINTILLA=ON`, include `mwfl/scintilla.h`, link
  `mwfl::scintilla`, and deploy `Scintilla.dll`. Treat positions as UTF-8 bytes.
- 2D drawing: link `mwfl::d2d`; keep document data independent of brushes and
  render targets.
- Swap chain: link `mwfl::d3d`; render on demand and handle every frame status.
- Image decode: link `mwfl::imaging`; initialize COM and enforce pixel budgets.

Never add optional SDK libraries to `mwfl::ui`. Do not retain callback-scoped
COM interfaces, render contexts, native notification pointers, or raw escape
hatches. UI integrations and callbacks remain on their creating thread.

Start browser changes from `examples/browser`, editor changes from
`examples/code_editor`, HWND hosting from `docs/recipes/native-host.md`, and
rendering changes from the corresponding reference application. Each supports
deterministic local tests without network or dialogs.

## Docking workspaces

- Start at `examples/docking_workspace` and
  `docs/tutorials/docking-workspace.md`; use
  `docs/recipes/docking-workspace.md` for focused edits.
- Stable `DockPanelId`, `DockGroupId`, `DockNodeId`, and
  `DockFloatingHostId` values are application identities. Never derive them
  from HWNDs, pointers, vector positions, or process-local state.
- `DockLayoutModel` owns metadata snapshots only. The application owns panel
  content, commands, documents, and HWND lifetimes.
- For every change: `Propose`, prepare/adopt with
  `DockNativeWorkspaceAdapter`, commit the model, then commit the adoption.
  Roll back native adoption if logical commit fails.
- `DockDragSession` separates target hit testing, proposal, preview, and commit.
  Escape, capture loss, destruction, stale state, rejection, reentrancy, and
  callback exceptions must preserve the original layout.
- Provide `DockKeyboardSession` whenever pointer targets exist. Floating hosts
  are owned auxiliary windows; panel HWNDs remain borrowed. Persistence is
  bounded, versioned, pointer-free, atomic where possible, and restored only
  after monitor recovery and graph validation.
- Focused local gate:
  `ctest --test-dir build/presets/vs2026-x64 -C Debug -R "mwfl.docking_"`;
  repeat with Release.
