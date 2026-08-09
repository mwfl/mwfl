# Handle a Scintilla notification

Link and deploy the optional component, load `Scintilla.dll`, and create the
editor before handling notifications. Route `WM_NOTIFY` through the window's
typed override:

```cpp
mwtl::EventResult OnNotify(const mwtl::NotifyEvent& event) override {
    if (!event.IsFrom(editor_)) return mwtl::EventResult::Propagate();
    const auto notification = editor_.DecodeNotification(event.header);
    if (!notification) return mwtl::EventResult::Propagate();

    if (notification->kind == mwtl::ScintillaNotificationKind::save_point_left)
        document_.MarkChanged();
    else if (notification->kind == mwtl::ScintillaNotificationKind::save_point_reached)
        document_.MarkSaved();
    else if (notification->kind == mwtl::ScintillaNotificationKind::modified &&
             notification->lines_added != 0)
        editor_.UpdateLineNumberMargin();
    return mwtl::EventResult::Propagate();
}
```

The decoded notification is a value; the native `NMHDR` and Scintilla event
payload remain borrowed for the duration of `WM_NOTIFY`. Public positions are
UTF-8 byte offsets. Keep document contents and save decisions in application
models; the control's save point only reports editor-buffer state.
