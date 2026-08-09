# Show taskbar progress

Create `TaskbarWindowIntegration` on the window's UI thread. Set a valid value
and total on `TaskbarProgressModel`, choose normal/error/paused state, and call
`Apply`. Clear progress and overlay state during teardown. When the registered
`TaskbarCreated` message arrives, call `Recreate` and reapply the application
model. Overlay HICON input is borrowed for the call.
