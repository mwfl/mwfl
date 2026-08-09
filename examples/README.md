# mwtl examples

The repository includes 35 focused executables. Each example is intentionally small enough to read as a complete recipe while still using the real `mwtl::Application` and recommended `mwtl::WindowBase` path. Common messages use typed C++20 event handlers rather than message-map macros. Notepad, Hot Corners, Settings, Explorer, Drawing, Image Viewer, Code Editor, and Browser are complete reference applications with dedicated model/native/GUI evidence.

| Directory | Target | Focus |
|---|---|---|
| `hello` | `mwtl_hello` | Smallest complete mwtl program |
| `application` | `mwtl_application_demo` | `mwtl::Application`, process entry, run result, and instance observation |
| `window` | `mwtl_window_demo` | `mwtl::WindowBase`, HWND access, typed event handlers, and native messages |
| `native_message` | `mwtl_native_message_demo` | Post and receive an application-defined `WM_APP` message |
| `keyboard` | `mwtl_keyboard_demo` | Handle keyboard input and close with Escape |
| `mouse` | `mwtl_mouse_demo` | Read native mouse client coordinates |
| `resize` | `mwtl_resize_demo` | Observe native pixel dimensions and size state |
| `timer` | `mwtl_timer_demo` | Own a window timer with `UiTimer` and `std::chrono` |
| `paint` | `mwtl_paint_demo` | Draw directly in a native `WM_PAINT` handler |
| `minmax` | `mwtl_minmax_demo` | Apply a minimum tracking size with `WM_GETMINMAXINFO` |
| `close_policy` | `mwtl_close_policy_demo` | Intercept close once, then delegate to the base policy |
| `window_state` | `mwtl_window_state_demo` | Observe restored, minimized, and maximized states |
| `dpi` | `mwtl_dpi_demo` | Per-window DPI context and the default `WM_DPICHANGED` rectangle policy |
| `window_options` | `mwtl_window_options_demo` | Class traits, styles, centered DIP client bounds, and native resources |
| `wait_aware` | `mwtl_wait_aware_demo` | A non-busy wait-aware pump with idle callbacks |
| `wakeup` | `mwtl_wakeup_demo` | A lifetime-safe worker-to-window wake token |
| `com_sta` | `mwtl_com_sta_demo` | Application-owned COM STA initialization and cleanup |
| `controls` | `mwtl_controls_demo` | All ten supported wrappers: text, buttons, grouping, choices, list, progress, slider, commands, and timer |
| `common_controls` | `mwtl_common_controls_demo` | TreeView, ListView, Toolbar, DateTimePicker, and every specialized Common Controls family |
| `self_drawn_host` | `mwtl_self_drawn_host_demo` | Worker-driven dirty frames, GDI paint, wake token, and wait-aware pump |
| `system_lifecycle` | `mwtl_system_lifecycle_demo` | Power, display, settings, IME, end-session, and accessibility hand-off messages |
| `hot_corners` | `mwtl_hot_corners_demo` | Multi-monitor hot corners using virtual-desktop coordinates and dwell detection |
| `form_binding` | `mwtl_form_binding_demo` | Live bindings, validation, model preview, and explicit `Push()`/`Pull()` flow |
| `commands` | `mwtl_commands_demo` | Shared command state across menu, toolbar, and keyboard accelerators |
| `desktop_integration` | `mwtl_desktop_integration_demo` | Modern dialogs, clipboard, file drops, task dialogs, and persistent placement |
| `document_state` | `mwtl_document_state_demo` | Dirty document state and safe user transition decisions |
| `notepad` | `mwtl_notepad` | Complete accessible Unicode SDI editor with atomic file operations |
| `appearance` | `mwtl_appearance_demo` | System/light/dark title bars, backdrops, corners, and accessibility helpers |
| `layout_gallery` | `mwtl_layout_gallery_demo` | Responsive nested rows, columns, overlays, sizing, alignment, and dynamic content |
| `property_sheet` | `mwtl_property_sheet_demo` | Persistent multi-page Settings with validation and Apply/OK/Cancel |
| `explorer` | `mwtl_explorer_demo` | Stable TreeView and virtual ListView in a complete Explorer-style shell |
| `drawing` | `mwtl_drawing_demo` | DPI-aware Direct2D drawing, resource recovery, and SVG export |
| `image_viewer` | `mwtl_image_viewer_demo` | WIC image decode, Fit/zoom/pan, color policy, and D2D recovery |
| `code_editor` | `mwtl_code_editor_demo` | Optional pinned Scintilla editor with Unicode files, search/replace, notifications, and dirty state |
| `browser` | `mwtl_browser_demo` | Optional pinned WebView2 browser with offline welcome, runtime diagnostics, navigation, and process recovery |

Configure with `MWTL_BUILD_EXAMPLES=ON`, then build one target or all targets:

```powershell
cmake -S . -B build/x64 -G "Visual Studio 18 2026" -A x64 -DMWTL_BUILD_EXAMPLES=ON
cmake --build build/x64 --config Debug --target mwtl_application_demo
cmake --build build/x64 --config Debug --target mwtl_window_demo
cmake --build build/x64 --config Debug --target mwtl_timer_demo
```

With the repository presets, the equivalent full examples/test build is:

```powershell
cmake --preset vs2026-x64
cmake --build --preset vs2026-x64-debug
```

All examples use the shared Per-Monitor V2 manifest in `example.manifest`.

Run a target from its configuration directory, for example:

```powershell
./build/x64/examples/paint/Debug/mwtl_paint_demo.exe
./build/x64/examples/native_message/Debug/mwtl_native_message_demo.exe
./build/x64/examples/timer/Debug/mwtl_timer_demo.exe
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

Coding agents should copy a complete example and modify it instead of merging
unrelated fragments. See `docs/agent-usage.md` for lifetime and threading rules.
