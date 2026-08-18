# Foundation migration after 0.1.9

The Foundation APIs are provisional. Current `main` deliberately refines the
0.1.9 slice before the next compatibility baseline.

| 0.1.9 usage | Current `main` |
|---|---|
| `ProcessExit::code` | `ProcessWaitResult::{status, exit_code}` |
| timeout/cancellation returned as process errors | `ProcessWaitStatus::TimedOut` / `Cancelled` |
| construct `PipeServer` and create the listener inside `Accept()` | `PipeServer::Create()` before launching a client |
| `ReadFrame()` returns only bytes | `PipeReadResult::{status, payload}` with one deadline |
| `WriteFrame()` returns `Result<void>` | `PipeWriteResult::status` |
| `DiagnosticPipeline::Write()` stops at the first sink error | all sinks run and return `DiagnosticWriteReport` |
| service callback receives only `stop_token` | `ServiceCallbacks` and shared `ServiceContext` |
| service start/stop timeout is a Win32 failure | `ServiceOperationResult::{status, snapshot}` preserves the final SCM state |
| construct `SecureBytes` from a movable vector | construct from an explicit byte span; the secure allocation itself is fixed and fully cleared |
| no stdin pipe on `ProcessBuilder` | `RedirectStdin()`, bounded `WriteInput()`, then `CloseInput()` |

The former Service callback overloads remain as adapters during development.
Machine mutation is never implicit: use `ServiceManager`,
`EventLogSourceManager`, `CredentialManager`, or `TaskScheduler` explicitly.
`mwfl::scheduler` is a new independently requested package component.
