# Public header reference

This index is the searchable map of the current mwtl public surface. Include the
smallest header that owns the API; use `mwtl/mwtl.h` only when broad convenience
is more important than compile-time isolation.

| Header | Primary API |
|---|---|
| `mwtl/application.h` | `Application`, `ApplicationOptions`, `RunApplication` |
| `mwtl/appearance.h` | `AppearanceOptions`, `ApplyWindowAppearance`, accessibility helpers |
| `mwtl/binding.h` | `ValueBinding`, `ValidationResult`, `ChangeGate` |
| `mwtl/command.h` | `Command`, `CommandShortcut`, `CommandSet` |
| `mwtl/command_controls.h` | `Toolbar`, `StatusBar`, `Rebar`, `Pager`, `Animation` |
| `mwtl/concepts.h` | `WindowLike`, `ControlLike` |
| `mwtl/control_batch.h` | checked range population helpers |
| `mwtl/control_host.h` | concise checked child-control construction |
| `mwtl/control_resources.h` | task dialogs, tooltips, image lists, flat scroll bars |
| `mwtl/controls.h` | core native child controls and `NativeControl` |
| `mwtl/desktop.h` | menus, accelerators, dialogs, clipboard, shell and placement helpers |
| `mwtl/document.h` | `DocumentState`, dirty tracking, and unsaved-transition decisions |
| `mwtl/dpi.h` | DIP geometry and `DpiContext` |
| `mwtl/error.h` | checked-operation `Error` and diagnostic context |
| `mwtl/events.h` | typed message events and `EventResult` |
| `mwtl/input_controls.h` | date, calendar, hot-key, IP, up-down and link controls |
| `mwtl/layout.h` | row/column/overlay retained layout |
| `mwtl/message_pump.h` | default and wait-aware message pumps |
| `mwtl/must.h` | `Must` and `MustInvoke` checked adapters |
| `mwtl/navigation_controls.h` | tree, list, header, tab and extended combo controls |
| `mwtl/recent_files.h` | bounded recent-file model and versioned registry persistence |
| `mwtl/single_instance.h` | named-instance ownership and bounded activation forwarding |
| `mwtl/text_file.h` | Unicode text detection, file stamps, and atomic save |
| `mwtl/text_history.h` | bounded text undo/redo history with saved-state tracking |
| `mwtl/text_search.h` | deterministic forward/backward find and replace-all helpers |
| `mwtl/timer.h` | move-only `UiTimer` |
| `mwtl/wakeup.h` | lifetime-safe worker-to-window notification |
| `mwtl/window.h` | `Window<T>`, `WindowBase`, typed dispatch and layout ownership |
| `mwtl/window_options.h` | window styles, bounds, resources and appearance |
| `mwtl/mwtl.h` | umbrella include |

Detailed semantics and examples are in [api.md](api.md). Ownership, error,
threading, and evolution rules are in [design.md](design.md) and
[stability.md](stability.md).
