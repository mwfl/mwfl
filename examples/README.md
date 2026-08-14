# mwfl examples

The repository includes 45 focused executables. GUI examples use the real `mwfl::Application` and recommended `mwfl::WindowBase` path; CLI examples use a Unicode console boundary without acquiring the UI component. Common messages use typed C++20 event handlers rather than message-map macros. Notepad, Hot Corners, Settings, Explorer, Drawing, Image Viewer, Printing, OLE Drag/Drop, Shell Integration, Code Editor, Browser, document/MDI/Ribbon workspaces, graphics interop, and Docking Workspace are complete reference applications with dedicated model/native/GUI evidence. Product applications extracted from this catalog live in separate repositories: [Folder Compare](https://github.com/mwfl/folder-compare), [Folder Explorer](https://github.com/mwfl/folder-explorer), [Hex Editor](https://github.com/mwfl/hex-editor), [Markdown Editor](https://github.com/mwfl/markdown-editor), [PDF Reader](https://github.com/mwfl/pdf-reader), and [SQLite Viewer](https://github.com/mwfl/sqlite-viewer).

| Directory | Target | Focus |
|---|---|---|
| `hello` | `mwfl_hello` | Smallest complete mwfl program |
| `application` | `mwfl_application_demo` | `mwfl::Application`, process entry, run result, and instance observation |
| `window` | `mwfl_window_demo` | `mwfl::WindowBase`, HWND access, typed event handlers, and native messages |
| `native_message` | `mwfl_native_message_demo` | Post and receive an application-defined `WM_APP` message |
| `keyboard` | `mwfl_keyboard_demo` | Handle keyboard input and close with Escape |
| `mouse` | `mwfl_mouse_demo` | Read native mouse client coordinates |
| `resize` | `mwfl_resize_demo` | Observe native pixel dimensions and size state |
| `timer` | `mwfl_timer_demo` | Own a window timer with `UiTimer` and `std::chrono` |
| `paint` | `mwfl_paint_demo` | Draw directly in a native `WM_PAINT` handler |
| `minmax` | `mwfl_minmax_demo` | Apply a minimum tracking size with `WM_GETMINMAXINFO` |
| `close_policy` | `mwfl_close_policy_demo` | Intercept close once, then delegate to the base policy |
| `window_state` | `mwfl_window_state_demo` | Observe restored, minimized, and maximized states |
| `dpi` | `mwfl_dpi_demo` | Per-window DPI context and the default `WM_DPICHANGED` rectangle policy |
| `window_options` | `mwfl_window_options_demo` | Class traits, styles, centered DIP client bounds, and native resources |
| `wait_aware` | `mwfl_wait_aware_demo` | A non-busy wait-aware pump with idle callbacks |
| `wakeup` | `mwfl_wakeup_demo` | A lifetime-safe worker-to-window wake token |
| `com_sta` | `mwfl_com_sta_demo` | Application-owned COM STA initialization and cleanup |
| `cli_basic` | `mwfl_cli_basic` | Unicode command routing, stdout/stderr, and stable exit codes |
| `cli_worker` | `mwfl_cli_worker` | Cooperative `std::jthread` and `std::stop_token` shutdown |
| `controls` | `mwfl_controls_demo` | All ten supported wrappers: text, buttons, grouping, choices, list, progress, slider, commands, and timer |
| `common_controls` | `mwfl_common_controls_demo` | TreeView, ListView, Toolbar, DateTimePicker, and every specialized Common Controls family |
| `self_drawn_host` | `mwfl_self_drawn_host_demo` | Worker-driven dirty frames, GDI paint, wake token, and wait-aware pump |
| `system_lifecycle` | `mwfl_system_lifecycle_demo` | Power, display, settings, IME, end-session, and accessibility hand-off messages |
| `hot_corners` | `mwfl_hot_corners_demo` | Multi-monitor outer hot corners using virtual-desktop topology and dwell detection |
| `form_binding` | `mwfl_form_binding_demo` | Live bindings, validation, model preview, and explicit `Push()`/`Pull()` flow |
| `commands` | `mwfl_commands_demo` | Shared command state across menu, toolbar, and keyboard accelerators |
| `desktop_integration` | `mwfl_desktop_integration_demo` | Modern dialogs, clipboard, file drops, task dialogs, and persistent placement |
| `document_state` | `mwfl_document_state_demo` | Dirty document state and safe user transition decisions |
| `notepad` | `mwfl_notepad` | Complete accessible Unicode SDI editor with atomic file operations |
| `document_workspace` | `mwfl_document_workspace` | Two-window multi-document editor with transfer, sessions, and coordinated close |
| `mdi_workspace` | `mwfl_mdi_workspace` | Native MDI frame/child composition with commands, coordinated close, and GUI self-test |
| `ribbon_workspace` | `mwfl_ribbon_workspace` | Windows Ribbon command routing, modes, contextual UI, and workspace integration |
| `graphics_interop` | `mwfl_graphics_interop` | Direct2D, Direct3D 11, WIC, and native-host interoperability in one application |
| `appearance` | `mwfl_appearance_demo` | System/light/dark title bars, backdrops, corners, and accessibility helpers |
| `layout_gallery` | `mwfl_layout_gallery_demo` | Responsive nested rows, columns, overlays, sizing, alignment, and dynamic content |
| `property_sheet` | `mwfl_property_sheet_demo` | Persistent multi-page Settings with validation and Apply/OK/Cancel |
| `explorer` | `mwfl_explorer_demo` | Stable TreeView and virtual ListView in a complete Explorer-style shell |
| `drawing` | `mwfl_drawing_demo` | DPI-aware Direct2D drawing, resource recovery, and SVG export |
| `image_viewer` | `mwfl_image_viewer_demo` | WIC image decode, Fit/zoom/pan, color policy, and D2D recovery |
| `printing` | `mwfl_printing_demo` | Shared pagination, preview zoom/navigation, printer settings, safe printing, and Esc cancellation |
| `ole_drag_drop` | `mwfl_ole_drag_drop_demo` | Unicode, files, custom OLE data, mouse drag/drop, and keyboard alternatives |
| `shell_integration` | `mwfl_shell_integration_demo` | Versioned settings, reversible association, Jump List, Recent, and taskbar recovery |
| `code_editor` | `mwfl_code_editor_demo` | Optional pinned Scintilla editor with Unicode files, search/replace, notifications, and dirty state |
| `browser` | `mwfl_browser_demo` | Optional pinned WebView2 browser with offline welcome, runtime diagnostics, navigation, and process recovery |
| `docking_workspace` | `mwfl_docking_workspace_demo` | IDE-style document/tool workspace with docking, floating, auto-hide, keyboard navigation, and session restore |

Configure with `MWFL_BUILD_EXAMPLES=ON`, then build one target or all targets:

```powershell
cmake -S . -B build/x64 -G "Visual Studio 18 2026" -A x64 -DMWFL_BUILD_EXAMPLES=ON
cmake --build build/x64 --config Debug --target mwfl_application_demo
cmake --build build/x64 --config Debug --target mwfl_window_demo
cmake --build build/x64 --config Debug --target mwfl_timer_demo
```

With the repository presets, the equivalent full examples/test build is:

```powershell
cmake --preset vs2026-x64
cmake --build --preset vs2026-x64-debug
```

All examples use the shared Per-Monitor V2 manifest in `example.manifest`.
The reference screenshots follow the repository's
[visual acceptance checklist](../docs/visual-acceptance.md): deterministic
content, readable system fonts, complete unclipped controls, and explicit DPI,
keyboard, theme, and High Contrast review for flagship applications.

Run a target from its configuration directory, for example:

```powershell
./build/x64/examples/paint/Debug/mwfl_paint_demo.exe
./build/x64/examples/native_message/Debug/mwfl_native_message_demo.exe
./build/x64/examples/timer/Debug/mwfl_timer_demo.exe
```

The examples remain native: every component is a real child HWND and direct Win32/WTL interoperability remains available. The wrappers provide ownership and typed ergonomics, not a closed rendering framework.

## Agent retrieval guide

| User intent | Start here | Primary symbols | Complexity |
|---|---|---|---|
| Smallest application | `hello` | `WindowBase`, `RunApplication` | starter |
| Native input form | `form_binding` | `ControlHost`, `ValueBinding`, `Column` | composed |
| Responsive UI | `layout_gallery` | `Row`, `Column`, `Overlay` | composed |
| Shared actions | `commands` | `CommandSet`, `Toolbar`, `Menu` | composed |
| Background notification | `wakeup` | `WindowWakeup`, `OnWakeup` | focused |
| Full worker lifecycle | `hot_corners` | wakeup, commands, persistence | reference app |
| Files and shell integration | `desktop_integration` | dialogs, clipboard, placement | composed |
| Native look and accessibility | `appearance` | appearance and accessibility helpers | composed |
| Persistent application settings | `property_sheet` | property pages, task dialogs, versioned state | reference app |
| Explorer-style desktop shell | `explorer` | stable navigation, virtual data, splitter, commands | reference app |
| Direct2D drawing | `drawing` | application-owned strokes, resource recovery, SVG export | reference app |
| Image viewing | `image_viewer` | bounded WIC decode, CPU pixels, zoom/pan, D2D bitmap cache | reference app |
| Source editing | `code_editor` | Scintilla runtime, UTF-8 byte positions, save points, notifications | reference app |
| Web content | `browser` | WebView2 runtime, async controller, navigation, process recovery | reference app |
| Docking workspace | `docking_workspace` | Stable panel identity, transactions, floating hosts, auto-hide, persistence | reference app |

Coding agents should copy a complete example and modify it instead of merging
unrelated fragments. See `docs/agent-usage.md` for lifetime and threading rules.
