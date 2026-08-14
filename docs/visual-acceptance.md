# Example visual acceptance

MWFL examples remain native Windows applications rather than a custom-skinned
widget set. Visual polish therefore means consistent system typography, clear
information hierarchy, useful populated states, correct layout at every DPI,
and complete keyboard and accessibility behavior.

## Screenshot contract

Every entry in `docs/examples.json` has a PNG under `docs/images/examples`.
Catalog verification rejects missing, truncated, undersized, or non-PNG
captures. A reference capture uses deterministic sample data, the example's
documented initial window bounds, and no transient menus, tooltips, focus
rectangles, or overlapping windows unless the transient UI is the capability
being demonstrated.

Flagship captures use the system color mode and current system message font.
The `--showcase` switch may suppress persisted user state so the same populated
composition can be reviewed repeatedly. Screenshot updates belong in the same
change as the corresponding UI change.

## Manual product pass

For Controls, Layout Gallery, Explorer, Notepad, Document Workspace, Docking
Workspace, Scintilla, and WebView2, inspect all of the following before release:

- 100%, 150%, and 200% scaling, including movement between monitors;
- the documented initial size and the minimum practical window size;
- system Light, Dark, and High Contrast modes;
- Tab/Shift+Tab order, default and cancel actions, arrow-key groups, and visible
  focus;
- 200% Windows text size where supported by the native control;
- populated, empty, disabled, validation, failure, and recovery states;
- no clipped labels, overlapping HWNDs, unreachable commands, placeholder
  content, or unexplained blank regions;
- accessible names for icon-only commands and controls without visible labels.

Record the Windows build, architecture, configuration, DPI, color mode, and any
intentional native rendering difference. Pixel-perfect equality is not a gate:
Windows versions legitimately change native control rendering. Review image
differences for structural regressions rather than accepting or rejecting them
solely by pixel count.

## Design rules for examples

- Use `WindowOptions::use_system_message_font` unless the example explicitly
  demonstrates an application-owned font.
- Prefer the 8/12/16/24/32 DIP spacing scale and align related fields.
- Use Mica or another supported backdrop only as a best-effort enhancement;
  content must remain legible when unavailable or overridden by High Contrast.
- Populate reference applications with realistic, clearly synthetic data.
- Keep custom drawing local to the example or an explicit drawing component;
  do not turn the public HWND layer into a closed renderer.
