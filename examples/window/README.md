# `mwfl::WindowBase` demo

![Window event example running on Windows](../../docs/images/examples/window.png)

This executable demonstrates all milestone-1 window integration points:

- derive from the concise `mwfl::WindowBase` form;
- initialize the attached HWND in `BuildUI()`;
- read the non-owning HWND with `GetHwnd()`;
- use typed C++20 convention handlers without message-map macros;
- send and receive native `WM_APP` messages without an enclosing abstraction;
- call `Close()` and let the single-main-window policy end the loop.

Click the client area to send the custom native message. Press Escape to close the window.

## Key code

Typed handlers can coexist with direct native-message handling:

```cpp
mwfl::EventResult OnMessage(const mwfl::WindowMessage& message) override {
    if (message.id == kShowNativeMessage) {
        SetTitle(L"Native WM_APP message received — press Esc to close");
        return mwfl::EventResult::Handled();
    }
    return mwfl::EventResult::Propagate();
}
```

See [`main.cpp`](main.cpp) for the complete HWND and close behavior.

## Build and run

```powershell
cmake --preset vs2026-x64
cmake --build --preset vs2026-x64-debug --target mwfl_window_demo
build\presets\vs2026-x64\examples\window\Debug\mwfl_window_demo.exe
```

Run `./scripts/verify-change.ps1 -Execute` after changing the example.
