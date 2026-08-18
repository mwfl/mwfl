# Foundation migration from 0.1.9 to 0.2.0

The 0.1.9 Foundation APIs were provisional. Version 0.2.0 deliberately refines
that slice and establishes the next public-preview compatibility baseline.

| 0.1.9 usage | 0.2.0 |
|---|---|
| `ProcessExit::code` | `ProcessWaitResult::{status, exit_code}` |
| timeout/cancellation returned as process errors | `OperationOutcome<T>` with `TimedOut` / `Cancelled` |
| construct a pipe server and create the listener inside `Accept()` | `PipeListener::Create()` before launching a client |
| `ReadFrame()` returns only bytes | `PipeReadResult::{status, payload}` with one deadline |
| `WriteFrame()` returns `Result<void>` | `PipeWriteResult::status` |
| `DiagnosticPipeline::Write()` stops at the first sink error | all sinks run and return `DiagnosticWriteReport` |
| free service callbacks receive only `stop_token` | derive `ServiceApplication` and share `ServiceContext` |
| service start/stop timeout is a Win32 failure | `ServiceOperationResult::{status, snapshot}` preserves the final SCM state |
| construct secure bytes from a movable vector | construct `SecureBuffer` from an explicit byte span; the secure allocation itself is fixed and fully cleared |
| no stdin pipe on `ProcessBuilder` | `RedirectStdin()`, bounded `WriteInput()`, then `CloseInput()` |

The former Service callback overloads remain as adapters during development.
Machine mutation is never implicit: use `ServiceManager`,
`EventLogSourceManager`, `CredentialManager`, or `TaskScheduler` explicitly.
`mwfl::scheduler` is a new independently requested package component.
