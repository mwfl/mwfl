# Windows Service application

`mwfl::service` is an opt-in Foundation component. Its 0.1.0 official slice
provides a single-service SCM host, the same callback under a console debug
host, and a testable status-transition model. Service installation and
management remain application/deployment-tool responsibilities.

## One application body, two hosts

The service business object should know only about cooperative cancellation:

```cpp
auto run_indexer = [](std::stop_token stop) -> mwfl::Result<void> {
    while (!stop.stop_requested()) ProcessOneBatch(stop);
    return {};
};
```

During development, the executable runs the same object as a console program:

```cpp
int wmain() {
    mwfl::ServiceDefinition service{
        L"MwflIndexingWorker", L"MWFL indexing worker"};
    return mwfl::RunServiceConsole(service, run_indexer);
}
```

Ctrl+C requests the same stop token that an SCM Stop or Shutdown control would
request. This path must not require service installation or elevation.

The production entry point selects the SCM host explicitly:

```cpp
int wmain() {
    mwfl::ServiceDefinition service{
        L"MwflIndexingWorker", L"MWFL indexing worker"};
    return mwfl::RunWindowsService(service, run_indexer);
}
```

No exception may cross `ServiceMain` or the control-handler callback. Starting,
running, stop-pending, and stopped status transitions remain observable and
carry native and service-specific exit codes.

## Explicit management boundary

Installation is a separate elevated management action rather than a startup
side effect:

```text
sc.exe create MwflIndexingWorker binPath= C:\Apps\worker.exe
sc.exe start MwflIndexingWorker
sc.exe stop MwflIndexingWorker
sc.exe delete MwflIndexingWorker
```

The repository's non-elevated tests cover the pure state machine and console
host. An elevated install/start/control/remove integration test is still a
roadmap gate and is not claimed by 0.1.0. The implementation supports only
`SERVICE_WIN32_OWN_PROCESS`; driver and shared-process services remain outside
scope.
