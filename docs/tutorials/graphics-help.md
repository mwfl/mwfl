# EMF, GDI+, taskbar, and Help integrations

These are optional traditional desktop integrations. Link `mwtl::graphics` for
enhanced metafiles and GDI+, or `mwtl::shell` for taskbar and Help. Core users
pay no link, COM, startup, or resource cost.

## Build and test

```powershell
cmake --preset vs2026-x64
cmake --build --preset vs2026-x64-debug --target mwtl_graphics_interop mwtl_shell_integration_demo
ctest --preset vs2026-x64-debug -R "^mwtl\.(graphics|graphics_interop_gui|help|shell_integration|shell_integration_gui)$"
```

Repeat with Release.

## Record, move, save, and play EMF

`RecordEnhancedMetafile` lends an HDC only during the callback and closes it on
success or exception. `EnhancedMetafile` is move-only and calls
`DeleteEnhMetaFile`; `Release` explicitly transfers that obligation.

```cpp
auto recorded = mwtl::RecordEnhancedMetafile(nullptr, L"report", [](HDC dc) {
    Rectangle(dc, 0, 0, 400, 240);
});
mwtl::SaveEnhancedMetafile(recorded.metafile, output_emf);
mwtl::PlayEnhancedMetafile(paint_dc, recorded.metafile, destination_pixels);
```

An explicit frame passed to recording uses 0.01 millimeter units. Playback
bounds use device pixels. Never retain the callback HDC.

## Export bounded PNG with GDI+

`ExportGdiPlusPng` creates a temporary ARGB bitmap, starts and stops an explicit
GDI+ token, contains drawing exceptions, and atomically replaces a sibling PNG.
Dimensions are limited to 32768 and 67,108,864 pixels.

```cpp
mwtl::ExportGdiPlusPng(path, 800, 600, [](Gdiplus::Graphics& graphics) {
    graphics.Clear(Gdiplus::Color(255, 255, 255, 255));
});
```

The `Graphics&` expires when the callback returns. GDI/GDI+ pens, brushes,
bitmaps, and transforms remain callback-owned.

## Taskbar recovery

Create `TaskbarWindowIntegration` on the window's STA UI thread. Progress,
overlay icons, thumbnail buttons, and tab HWNDs use stable application IDs.
Icons and HWNDs are borrowed for each call. After `TaskbarCreated`, call
`Recreate` and reapply the complete application-owned state. Clear reversible
state before destroying the window.

## Safe contextual Help

Use `HelpRequest` rather than building a command line. CHM and local HTML files
must be absolute existing local files with matching extensions. HTTPS is the
only accepted network scheme. UNC paths, traversal, credentials, whitespace,
quotes, controls, and malformed topics are rejected.

```cpp
mwtl::HelpRequest request{
    mwtl::HelpTargetKind::https_uri, {}, L"https://example.com/help", L"#editing"};
auto result = mwtl::LaunchHelp(owner, request);
```

Cancellation, missing content, unavailable HTML Help, and native failure are
different statuses. Tests use `LaunchHelpWithBackend` for deterministic offline
success, cancellation, and exception paths without opening a browser.
