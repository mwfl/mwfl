# Public API contract audit for 0.1

This is the release-freeze audit of every installed public header. It records
the contract that reviewers must preserve after `v0.1.0`; it does not replace
the detailed per-symbol reference. The audit was performed against the header
set installed by CMake and the independent-header compile targets.

## Contract vocabulary

- **Owned** native resources are move-only or have one documented destroying
  wrapper. **Borrowed** handles and callback views never transfer ownership.
- HWND and UI wrappers are used and destroyed on their creating thread unless
  a model is explicitly documented as thread-independent. COM surfaces state
  their required apartment.
- `bool` is limited to atomic yes/no operations where cancellation and failure
  do not need separate representation. System boundaries with meaningful
  cancellation, partial success, or recovery use a status/result type.
- No exception crosses a Win32 or COM callback. Expected runtime failures are
  returned; setup failures may be promoted with `Must` at the application
  callback boundary.
- `GetHwnd`, `Get`, and raw interface accessors return borrowed escape hatches
  unless a method explicitly says `Release` or transfer.

## Reviewed header families

| Layer | Headers | Stability | Ownership and lifetime | Thread/apartment | Failure contract |
|---|---|---|---|---|---|
| Core lifecycle | `application.h`, `message_pump.h`, `window.h`, `window_options.h`, `events.h`, `timer.h`, `wakeup.h`, `async.h` | Stable | Windows/timers own native lifetime; wakeup tickets and async tickets never keep the owner alive | Creating UI thread; worker use only through documented tickets/wakeup | Callback exceptions are contained; message-loop and initialization failures are explicit |
| Core controls | `control_host.h`, `control_batch.h`, `controls.h`, `input_controls.h`, `navigation_controls.h`, `command_controls.h`, `control_resources.h`, `selection.h`, `splitter.h`, `native_host.h` | Stable | Wrappers own created HWNDs; hosts/layout/adapters borrow application controls; image/menu transfer is explicit | Creating UI thread | Creation/mutation is checked; task dialogs and native operations with cancellation use structured results |
| Core layout and diagnostics | `layout.h`, `dpi.h`, `concepts.h`, `error.h`, `must.h`, `mwfl.h` | Stable | Layout retains native identity, not C++ wrapper ownership | UI-thread application; pure geometry is thread-independent | DIPs and pixels are explicit; `Must` is the source-located setup escalation path |
| Application models | `binding.h`, `command.h`, `document.h`, `document_workspace.h`, `document_coordination.h`, `document_session.h`, `document_tabs.h`, `tab_workspace.h`, `text_file.h`, `text_history.h`, `text_search.h`, `recent_files.h`, `settings_store.h`, `single_instance.h` | `command.h` provisional; remainder stable | Models own copied metadata, never application documents/pages; persistence is bounded and pointer-free | Coordinate models from one UI thread unless a pure helper states otherwise | Cancellation, stale state, partial restore, duplicate identity and I/O failure remain distinguishable |
| Application desktop | `desktop.h`, `dialog.h`, `appearance.h`, `property_sheet.h`, `tray_icon.h` | Appearance provisional; remainder stable | Dialog/tray/resource wrappers own what they create and borrow owners/icons as documented | Creating UI thread; COM dialogs use the caller apartment | User cancellation is distinct from system failure; appearance is explicitly best-effort |
| Advanced workspaces | `docking_workspace.h`, `docking_native.h`, `docking_drag.h`, `docking_preview.h`, `docking_keyboard.h`, `docking_floating.h`, `docking_auto_hide.h`, `docking_monitor.h`, `docking_session.h`, `mdi.h` | Stable optional/focused components | Logical stable-ID models own metadata; native adapters borrow application HWNDs; floating hosts own only their host HWND | Creating UI thread for native projection; pure proposals/monitor policy are model code | Mutations use propose/prepare/adopt/rollback/commit and preserve ownership on failure |
| Advanced OS integration | `printing.h`, `printing_native.h`, `printing_settings.h`, `ole_data.h`, `ole_drag_drop.h`, `settings_store.h`, `file_association.h`, `shell_integration.h`, `help.h`, `ribbon.h`, `graphics.h` | Stable optional/focused components | HDC/callback views are borrowed; COM/STGMEDIUM, EMF, GDI+, registration and shell ownership are explicit | Creating STA/UI thread where required; callbacks may be reentrant | Structured results preserve unsupported, cancelled, conflict, partial and failed states; cleanup remains balanced |
| Optional rendering and editors | `d2d_host.h`, `d3d_host.h`, `imaging.h`, `webview2.h`, `scintilla.h` | Stable optional components | GPU/COM callback objects are borrowed; decoded CPU pixels and host HWNDs have explicit owners | Creating UI thread; WebView2 requires STA | Device/runtime loss and unavailable components are recoverable states; strict encoding rejects malformed input |

## Audit disposition

The reviewed surface has one canonical ownership path per native resource and
no public signature names `mwfl::detail`. Direct control `Create` methods remain
intentional low-level checked operations, while `ControlHost` is the canonical
ordinary construction path. Existing `bool` mutations are atomic operations;
APIs with cancellation, partial completion, stale data, or recoverable platform
failure already expose status/result types. No compatibility alias is added for
the unpublished pre-0.1 milestone names.

Verification evidence is provided by the independent-header targets,
`mwfl.api_surface`, `mwfl.architecture_boundaries`, the public baseline consumer,
package consumers, lifecycle tests, native resource leak test, and the complete
Debug/Release release-candidate runs recorded in
[release-readiness.md](release-readiness.md).
