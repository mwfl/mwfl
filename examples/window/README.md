# `mwfl::WindowBase` demo

This executable demonstrates all milestone-1 window integration points:

- derive from the concise `mwfl::WindowBase` form;
- initialize the attached HWND in `BuildUI()`;
- read the non-owning HWND with `GetHwnd()`;
- use typed C++20 convention handlers without message-map macros;
- send and receive native `WM_APP` messages without an enclosing abstraction;
- call `Close()` and let the single-main-window policy end the loop.

Click the client area to send the custom native message. Press Escape to close the window.
