# Windows Service application design preview

Windows Service support is an approved opt-in foundation component, but
`mwfl::service` is not implemented yet. The examples below are the F2 API
acceptance shape, not currently compilable public API. Keeping them here makes
the intended developer experience reviewable without pretending the symbols
already ship.

## One application body, two hosts

The service business object should know only about cooperative cancellation:

```cpp
class IndexingWorker {
public:
    int Run(std::stop_token stop) {
        while (!stop.stop_requested()) ProcessOneBatch(stop);
        return 0;
    }
};
```

During development, the executable runs the same object as a console program:

```cpp
int wmain() {
    return mwfl::RunServiceConsole<IndexingWorker>(L"MWFL indexing worker");
}
```

Ctrl+C requests the same stop token that an SCM Stop or Shutdown control would
request. This path must not require service installation or elevation.

The production entry point selects the SCM host explicitly:

```cpp
int wmain() {
    return mwfl::RunService<IndexingWorker>({
        .name = L"MwflIndexingWorker",
        .accepted_controls = mwfl::ServiceControls::stop |
                             mwfl::ServiceControls::shutdown,
    });
}
```

No exception may cross `ServiceMain` or the control-handler callback. Starting,
running, stop-pending, and stopped status transitions remain observable and
carry native and service-specific exit codes.

## Explicit management CLI

Installation is a separate elevated management action rather than a startup
side effect:

```text
mwfl-service-tool install --name MwflIndexingWorker --binary C:\Apps\worker.exe
mwfl-service-tool query   --name MwflIndexingWorker
mwfl-service-tool start   --name MwflIndexingWorker
mwfl-service-tool stop    --name MwflIndexingWorker
mwfl-service-tool remove  --name MwflIndexingWorker
```

The F2 integration test will use a unique test name, verify start/control/stop,
remove the service in success and failure paths, and confirm that no SCM state
is left behind. The first implementation supports only
`SERVICE_WIN32_OWN_PROCESS`; driver and shared-process services remain outside
scope.
