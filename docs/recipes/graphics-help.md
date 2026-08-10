# EMF, GDI+, taskbar, and Help recipes

- `EnhancedMetafile` owns one HENHMETAFILE; `Release` transfers deletion duty.
- Recording HDCs and GDI+ `Graphics` exist only during their callbacks.
- Validate EMF frame units (0.01 mm), playback pixel rectangles, dimensions,
  pixel counts, and every GDI/GDI+ status.
- Export PNG through the bounded sibling-temporary helper.
- Recreate taskbar COM state after `TaskbarCreated`, then reapply progress,
  overlays, thumbnail buttons, registered tabs, and active tab.
- Clear taskbar state before HWND teardown.
- Validate Help first; accept absolute local CHM/HTML or HTTPS only.
- Use the injectable Help backend in offline tests; never construct a shell
  command or persist process/native handles.

See `docs/tutorials/graphics-help.md`, `examples/graphics_interop`, and
`examples/shell_integration`.
