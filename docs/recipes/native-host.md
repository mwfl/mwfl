# Host a third-party child HWND

Use `NativeHost` when a library creates an ordinary child HWND and you need
layout, focus, notification forwarding, and deterministic teardown without
building a library-specific wrapper.

```cpp
mwfl::NativeHost host_;

ui.Add(host_);
HWND child = CreateWindowExW(0, third_party_class, L"", WS_CHILD | WS_VISIBLE,
                             0, 0, 1, 1, host_.GetHwnd(), nullptr, instance, nullptr);
mwfl::Must(host_.Attach(child), "attach third-party child");
SetLayout(mwfl::Column().Add(host_, mwfl::Stretch()));
```

The host owns its container HWND and borrows one direct-child HWND. Normal
Win32 parent destruction destroys an attached child. `Detach` stops management
and returns the borrowed HWND without destroying or reparenting it.

Both windows must belong to the same UI thread and process. The host resizes
the child to its client area, transfers focus, and forwards child/descendant
`WM_NOTIFY`, control `WM_COMMAND`, and `WM_CONTEXTMENU` synchronously to its
parent. Do not retain forwarded notification pointers after the message ends.

Use a specialized host such as `WebView2Host`, `D2DHost`, or `D3DHost` when the
dependency does not expose an attachable child HWND or needs extra resource
lifecycle management.
