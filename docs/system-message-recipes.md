# Native system-message recipes

mwfl intentionally leaves system semantics with the application. Rare messages
can share the typed raw-message fallback without introducing a macro map:

```cpp
mwfl::EventResult OnMessage(const mwfl::WindowMessage& message) {
    switch (message.id) {
    case WM_QUERYENDSESSION:
        return mwfl::EventResult::Handled(TRUE);
    case WM_POWERBROADCAST:
    case WM_SETTINGCHANGE:
        RefreshSystemState();
        return mwfl::EventResult::Propagate();
    default:
        return mwfl::EventResult::Propagate();
    }
}
```

Use `EventResult::Propagate()` when `DefWindowProcW` must retain its behavior.
Preserve the documented LRESULT for messages such as `WM_QUERYENDSESSION` and
`WM_GETOBJECT` rather than reducing handlers to notifications.

| Message | Consumer responsibility |
|---|---|
| `WM_PAINT`, `WM_SIZE` | own renderer resources, resize and invalidation |
| `WM_DPICHANGED` | mwfl applies the suggested rect by default; refresh consumer DPI resources |
| `WM_DISPLAYCHANGE` | re-query monitor/display capabilities |
| `WM_SETTINGCHANGE` | re-query high contrast and other relevant settings |
| `WM_POWERBROADCAST` | pause/resume consumer devices and background services |
| `WM_GETMINMAXINFO` | return native pixel tracking constraints for current DPI |
| `WM_IME_*`, `WM_CHAR` | own candidate position and committed UTF-16 input |
| `WM_SETCURSOR`, mouse wheel | own hit testing and screen/client coordinate conversion |
| `WM_GETOBJECT` | return the consumer-owned UI Automation provider |
| `WM_QUERYENDSESSION`, `WM_ENDSESSION` | save product state and return the required consent value |

See `examples/system_lifecycle`, `examples/self_drawn_host`, and the C++20
the lifecycle and modern API tests. All handlers run inside the same exception-safe
WTL WindowProc boundary as other user message handlers. Existing WTL message
maps remain supported for alternate map IDs and specialized chaining.
