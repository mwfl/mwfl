# Accessibility and keyboard checklist

mwfl uses native HWND controls, so standard controls retain Windows keyboard,
high-contrast, and accessibility behavior. Applications are still responsible
for meaningful names, navigation order, grouping, and validation feedback.

## Build-time review

- Put controls in logical creation/tab order and retain `WS_TABSTOP` only where
  focus is useful.
- Give every input a visible label or call `SetAccessibleName` when visible text
  is intentionally absent.
- Use `&` mnemonics in labels and commands, without duplicating a mnemonic in
  the same scope.
- Establish a default button with `SetDialogDefaultButton`; handle Escape with a
  clear cancel/close policy.
- Do not convey enabled, checked, selected, warning, or error state by color
  alone.
- Keep layout minimums usable at 200% and 300% DPI and with longer translated
  text.

## Release validation

1. Navigate the complete application using Tab, Shift+Tab, arrows, Alt
   mnemonics, Enter, Space, and Escape.
2. Inspect with Narrator and Accessibility Insights: names, roles, states, focus
   order, and live status changes must be understandable.
3. Enable Windows high contrast. mwfl suppresses cosmetic DWM preferences, but
   application-owned colors and GDI drawing must also remain legible.
4. Validate 100%, 200%, and 300% per-monitor DPI, including moving the window
   between monitors.
5. Repeat important workflows with animations disabled and keyboard cues on.

Automated HWND tests protect basic naming, appearance fallback, and layout
invariants. They do not replace testing with real assistive technology.
