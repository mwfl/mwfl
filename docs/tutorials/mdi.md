# Build an optional legacy MDI application

Use `DocumentWorkspaceModel` and native tabs for new multi-document products.
Use the optional MDI component only when compatibility with the traditional
Windows MDI interaction model is itself a requirement. MDI is not included by
`mwfl::mwfl` and never becomes global application architecture.

## 1. Build the reference

```powershell
cmake --preset vs2026-x64
cmake --build --preset vs2026-x64-debug --target mwfl_mdi_workspace
ctest --preset vs2026-x64-debug -R "^mwfl\.mdi_(model|native|workspace_gui)$"
```

Repeat with `vs2026-x64-release`. The manifest makes the reference Windows 10+
and Per-Monitor V2 aware.

## 2. Link only when needed

```cmake
find_package(mwfl 0.1 CONFIG REQUIRED COMPONENTS mdi)
target_link_libraries(my_app PRIVATE mwfl::mdi)
```

`MdiWorkspaceModel` owns stable IDs, titles, ordering, active selection, dirty
flags, and close policy. The application still owns documents, undo history,
views, and save operations.

```cpp
mwfl::MdiWorkspaceModel model;
model.Add({{1}, L"Document 1"});
model.Add({{2}, L"Document 2", true});
```

## 3. Create the native host and children

Create `MdiHost` on the frame's UI thread. It owns its `MDICLIENT` and MDI child
HWNDs while borrowing the frame, model, menu, and message callbacks.

```cpp
mwfl::MdiHost host;
host.Create(frame, model, window_menu);
host.Resize(client_bounds_pixels);
host.CreateChild({1}, {
    WS_VISIBLE | WS_OVERLAPPEDWINDOW,
    [](HWND child, UINT message, WPARAM, LPARAM)
        -> std::optional<LRESULT> {
        if (message == WM_PAINT) { PaintDocument(child); return 0; }
        return std::nullopt;
    }});
```

Callbacks are synchronous and reentrant. They may borrow the HWND during the
call but must not throw through a window procedure. mwfl captures exceptions;
retrieve them with `TakeCallbackException`.

## 4. Route commands without a global current document

At invocation time snapshot the active stable ID:

```cpp
auto routed = mwfl::RouteMdiActiveChild(model, [&](mwfl::MdiChildId id) {
    documents.at(id.value).Save();
});
```

If the callback reentrantly activates another child, the result is
`active_child_changed`. Menus and accelerators call the same handlers. Pass each
message through the normal application accelerator table, then
`MdiHost::Translate`, and use `FrameDefault` for unhandled frame messages.

## 5. Arrange, transfer, and close safely

`Activate`, `Arrange`, and `Move` use stable IDs. `TransferTo` copies into the
destination model before removing the source, so allocation failure leaves the
source intact. Native HWND transfer is deliberately application-controlled;
traditional MDI children are tied to one MDICLIENT.

Before closing any dirty documents, collect every save/discard/cancel choice.
Do not destroy one child while another decision is still pending. A cancel or
failed save leaves every child open. Once all decisions succeed, call `Close`
for the planned IDs. `closable == false` is enforced unless the application
uses the explicit force path during committed shutdown.

## 6. Teardown

Stop callbacks, close children, call `host.Destroy`, destroy the frame, and end
the UI thread. `Destroy` is idempotent and does not post a process quit message.
The reference application's `--self-test` proves cancel atomicity, command
routing, activation, arrangement, create/close, and complete shutdown.
