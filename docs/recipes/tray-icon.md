# Add a notification-area icon

Use `mwfl/tray_icon.h` for a background utility whose status and commands must
remain available from the Windows notification area.

1. Give the icon a stable, application-owned GUID. Do not generate a new GUID
   on every launch.
2. Choose a nonzero callback ID no larger than 65,535 and a private `WM_APP`
   message.
3. Keep the owner HWND and shared HICON alive until `Remove`.
4. In `OnMessage`, recover first from `TaskbarCreated`, then decode ordinary
   tray callbacks.
5. Build the context menu from the application's `CommandSet` and post the
   selected command back through `WM_COMMAND`.
6. Call `Remove` before destroying the owner. Destruction repeats this safely.

```cpp
constexpr UINT kTrayMessage = WM_APP + 42;
constexpr GUID kTrayGuid{0x6ca0ba3d, 0xb00e, 0x483f,
                         {0xa0, 0xe3, 0x95, 0xe2, 0xb1, 0xad, 0x5c, 0x8f}};

void BuildUI() override {
    if (!tray_.Add({.owner = GetHwnd(),
                    .id = 1,
                    .callback_message = kTrayMessage,
                    .identity = kTrayGuid,
                    .icon = ::LoadIconW(nullptr, IDI_APPLICATION),
                    .tooltip = L"My utility"})) {
        throw std::runtime_error("tray icon creation failed");
    }
}

mwfl::EventResult OnMessage(const mwfl::WindowMessage& message) override {
    if (tray_.IsTaskbarCreated(message)) {
        static_cast<void>(tray_.Recreate());
        return mwfl::EventResult::Handled();
    }
    const auto event = tray_.Decode(message);
    if (!event) return mwfl::EventResult::Propagate();
    if (event->kind == mwfl::TrayIconEventKind::context_menu) {
        ShowCommandMenu(event->screen_position);
    }
    return mwfl::EventResult::Handled();
}

mwfl::EventResult OnClose() override {
    tray_.Remove();
    return mwfl::EventResult::Propagate();
}
```

`Add` selects notification protocol v4, so callback coordinates arrive in
screen pixels and keyboard activation is distinguishable. A failed Explorer
recreation remains `recovery_pending`; retry explicitly rather than assuming
the icon is present. Updates modify cached tooltip/icon/visibility state only
after the shell accepts them. See `examples/hot_corners/main.cpp` for the full
command, notification, persistence, and deterministic-shutdown composition.
