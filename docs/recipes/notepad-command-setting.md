# Add a command and persistent setting to Notepad

Canonical source: `examples/notepad/main.cpp`. Compiled Agent baseline:
`agent-evals/fixtures/notepad_setting.cpp`.

This recipe adds one **Always on Top** preference without duplicating behavior
between a callback, menu item, and persisted value.

## Contract

- Give the command a stable `ControlId` outside the control-host auto-ID range.
- Store the runtime value in one `bool` and mirror it into `Command::SetChecked`.
- Route `OnCommand` through the owning `CommandSet`.
- Call `Menu::UpdateCommand` after changing checked state.
- Apply the native behavior with the public `GetHwnd()` escape hatch.
- Store preferences below an application-owned, versioned HKCU key. The Notepad
  example uses `Software\\mwtl\\Notepad\\1` and a `REG_DWORD` named
  `AlwaysOnTop`.
- A missing value means “use the default”; a failed preference write must not
  corrupt the open document or terminate the application.

## Change sequence

1. Add `constexpr mwtl::ControlId kAlwaysOnTop{718};`.
2. Load the DWORD before building commands.
3. Add one `Command` whose callback toggles the model value, calls
   `SetWindowPos(HWND_TOPMOST/HWND_NOTOPMOST)`, updates checked state, and saves
   the DWORD.
4. Append that command to a `View` popup and retain the attached `Menu` as a
   window member so later updates remain valid.
5. Keep `OnCommand` as `return commands_.Dispatch(event);` after handling any
   control notifications.
6. Extend `mwtl.notepad_gui` or an equivalent GUI test to inspect both
   `Command::IsChecked()` and `WS_EX_TOPMOST` after each transition.

Do not invent a settings service, put a second callback directly on the menu,
or treat a failed optional preference write as permission to discard document
content.
