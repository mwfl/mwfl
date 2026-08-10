# Handle a Scintilla notification

Link and deploy the optional component, load `Scintilla.dll`, and create the
editor before handling notifications. Route `WM_NOTIFY` through the window's
typed override:

```cpp
mwfl::EventResult OnNotify(const mwfl::NotifyEvent& event) override {
    if (!event.IsFrom(editor_)) return mwfl::EventResult::Propagate();
    const auto notification = editor_.DecodeNotification(event.header);
    if (!notification) return mwfl::EventResult::Propagate();

    if (notification->kind == mwfl::ScintillaNotificationKind::save_point_left)
        document_.MarkChanged();
    else if (notification->kind == mwfl::ScintillaNotificationKind::save_point_reached)
        document_.MarkSaved();
    else if (notification->kind == mwfl::ScintillaNotificationKind::modified &&
             notification->lines_added != 0)
        editor_.UpdateLineNumberMargin();
    return mwfl::EventResult::Propagate();
}
```

The decoded notification is a value; the native `NMHDR` and Scintilla event
payload remain borrowed for the duration of `WM_NOTIFY`. Public positions are
UTF-8 byte offsets. Keep document contents and save decisions in application
models; the control's save point only reports editor-buffer state.
