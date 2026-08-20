# Foundation components in 0.1.0

The first official `v0.1.0` release includes independently acquired Foundation
components. Existing `mwfl::ui` consumers do not acquire Service, process, IPC,
diagnostics, security, deployment, or scheduling behavior.

| Release | Component or integration | Runnable example |
|---|---|---|
| 0.1.0 | `mwfl::core`: errors, results, handles, Unicode, cancellable wait | `core_foundation` |
| 0.1.0 | `mwfl::service`: state model, console host, SCM dispatcher | `service_host` |
| 0.1.0 | `mwfl::process`: structured arguments, launch, wait, exit | `process_runner` |
| 0.1.0 | `mwfl::ipc`: bounded local Named Pipe frames | `ipc_framed` |
| 0.1.0 | `mwfl::diagnostics`: structured redaction and bounded sinks | `diagnostics_pipeline` |
| 0.1.0 | `mwfl::security`: current-user DPAPI and cleared plaintext | `security_dpapi` |
| 0.1.0 | `mwfl::deployment`: package identity and restart registration | `deployment_restart` |
| 0.1.0 | cross-component controller/worker stack | `foundation_stack` |
| 0.1.0 | combined acquisition and compatibility smoke | `foundation_overview` |

All examples support `--self-test`. System mutation remains explicit: the
Service example never installs itself, IPC names are process-unique, DPAPI uses
the current user, diagnostics uses a bounded temporary file, and deployment
does not download or install software.

## Production baseline

Version 0.1.0 includes the productionized components and independent Scheduler
component. The focused examples are `process_supervisor`, `ipc_secure`,
`service_management`, `diagnostics_native`, `security_credentials`,
`deployment_handoff`, and `scheduled_task`. The multi-file
`supervised_worker` reference application demonstrates their complete vertical
composition without adding a general RPC or updater framework.

Runtime code never installs or repairs machine state implicitly. Service,
Event Log source, credential, and scheduled-task mutation remains explicit,
idempotent, reversible, and separate from ordinary self-tests.
