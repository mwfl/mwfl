# Ribbon recipes

- Link `mwfl::ribbon`; do not add Ribbon headers to the core umbrella.
- Generate BML, header, and RC with SDK `uicc.exe`; embed the RC in the EXE.
- Map stable Ribbon IDs to stable `ControlId` values with `RibbonCommandModel`.
- Initialize COM as STA, then `Create`, `Load`, set modes/context, and invalidate.
- Keep recent-item IDs stable and persist only plain application values.
- Use `GetHeight` for content layout and invalidate after DPI/theme changes.
- Treat `GetFramework` as borrowed and destroy the host before COM shutdown.
- Support `RibbonHostStatus::unavailable` with a menu/toolbar fallback.

See `docs/tutorials/ribbon.md` and `examples/ribbon_workspace`.
