# Register an OLE drop target

Start the application with `ComApartment::ole_sta`, create callbacks with
`CreateOleDropTarget`, then keep both the COM target and returned
`OleDropTargetRegistration` alive. Callbacks run reentrantly on the UI thread;
copy message-scoped data and contain exceptions. Revoke before destroying the
HWND. Repeated `Revoke` is safe and reports `not_registered`.
