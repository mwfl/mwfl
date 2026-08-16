# Public header reference

This index is the searchable map of the current mwfl public surface. Include the
smallest header that owns the API; use `mwfl/mwfl.h` only when broad convenience
is more important than compile-time isolation.

| Header | Primary API |
|---|---|
| `mwfl/application.h` | `Application`, `ApplicationOptions`, `RunApplication` |
| `mwfl/appearance.h` | `AppearanceOptions`, resolved `AppearanceState`/palette, native theme propagation, accessibility helpers |
| `mwfl/binding.h` | `ValueBinding`, `ValidationResult`, `ChangeGate` |
| `mwfl/command.h` | `Command`, `CommandShortcut`, `CommandSet` |
| `mwfl/command_controls.h` | `Toolbar`, `StatusBar`, `Rebar`, `Pager`, `Animation` |
| `mwfl/concepts.h` | `WindowLike`, `ControlLike` |
| `mwfl/control_batch.h` | checked range population helpers |
| `mwfl/control_host.h` | concise checked child-control construction |
| `mwfl/control_resources.h` | task dialogs, tooltips, image lists, flat scroll bars |
| `mwfl/controls.h` | core native child controls and `NativeControl` |
| `mwfl/dialog.h` | retained-layout modal and modeless native dialogs with structured results |
| `mwfl/desktop.h` | menus, accelerators, dialogs, clipboard, shell and placement helpers |
| `mwfl/document.h` | `DocumentState`, dirty tracking, and unsaved-transition decisions |
| `mwfl/docking_workspace.h` | stable-ID panel/group/split graph and transactional mutations |
| `mwfl/docking_native.h` | rollback-safe borrowed-HWND docking projection |
| `mwfl/docking_drag.h` | cancellable target/proposal/commit drag sessions |
| `mwfl/docking_preview.h` | nonactivating DPI-aware native docking preview |
| `mwfl/docking_keyboard.h` | spatial and sequential accessible docking navigation |
| `mwfl/docking_floating.h` | owned auxiliary floating host with borrowed content |
| `mwfl/docking_auto_hide.h` | deterministic auto-hide timing and pin policy |
| `mwfl/docking_monitor.h` | injectable floating-placement monitor recovery |
| `mwfl/docking_session.h` | bounded versioned pointer-free layout persistence |
| `mwfl/dpi.h` | DIP geometry and `DpiContext` |
| `mwfl/error.h` | checked-operation `Error` and diagnostic context |
| `mwfl/events.h` | typed message events and `EventResult` |
| `mwfl/graphics.h` | move-only enhanced metafiles, explicit GDI+ startup, and bounded PNG export |
| `mwfl/help.h` | validated CHM/local HTML/HTTPS Help requests and structured launch results |
| `mwfl/input_controls.h` | date, calendar, hot-key, IP, up-down and link controls |
| `mwfl/layout.h` | row/column/overlay retained layout |
| `mwfl/message_pump.h` | message loop, pre-translate filters, default and wait-aware message pumps |
| `mwfl/mdi.h` | optional stable-ID legacy MDI model and owned native MDICLIENT/children |
| `mwfl/must.h` | `Must` and `MustInvoke` checked adapters |
| `mwfl/navigation_controls.h` | tree, list, header, tab and extended combo controls |
| `mwfl/property_sheet.h` | stable page state plus modal/modeless native property sheets |
| `mwfl/recent_files.h` | bounded recent-file model and versioned registry persistence |
| `mwfl/ribbon.h` | optional pointer-free Ribbon command model and STA Windows Ribbon host |
| `mwfl/shell_integration.h` | Jump Lists, taskbar progress/overlays/thumbnail buttons/tabs, and Explorer recovery |
| `mwfl/single_instance.h` | named-instance ownership and bounded activation forwarding |
| `mwfl/tab_workspace.h` | stable-ID tab state, selection, ordering, and dirty metadata |
| `mwfl/splitter.h` | native two-pane composition, keyboard/mouse movement, constraints, and pure geometry |
| `mwfl/text_file.h` | Unicode text detection, file stamps, and atomic save |
| `mwfl/text_history.h` | bounded text undo/redo history with saved-state tracking |
| `mwfl/text_search.h` | deterministic forward/backward find and replace-all helpers |
| `mwfl/timer.h` | move-only `UiTimer` |
| `mwfl/tray_icon.h` | RAII notification-area identity, events, notifications, and Explorer recovery |
| `mwfl/wakeup.h` | lifetime-safe worker-to-window notification |
| `mwfl/window.h` | `Window<T>`, `WindowBase`, typed dispatch and layout ownership |
| `mwfl/window_options.h` | window styles, bounds, DPI-aware system font, resources and appearance |
| `mwfl/mwfl.h` | umbrella include |

Detailed semantics and examples are in [api.md](api.md). Ownership, error,
threading, and evolution rules are in [design.md](design.md) and
[stability.md](stability.md). The release-freeze classification and consolidated
failure/ownership/threading review are in
[public-api-contract-audit.md](public-api-contract-audit.md).
