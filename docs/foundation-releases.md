# Foundation release train

The 0.1.x public-preview train adds backward-compatible, independently
requested Foundation components. Existing `mwfl::ui` consumers do not acquire
Service, process, IPC, diagnostics, security, or deployment behavior.

| Version | Component or integration | Runnable example |
|---|---|---|
| 0.1.1 | `mwfl::core`: errors, results, handles, Unicode, cancellable wait | `core_foundation` |
| 0.1.2 | `mwfl::service`: state model, console host, SCM dispatcher | `service_host` |
| 0.1.3 | `mwfl::process`: structured arguments, launch, wait, exit | `process_runner` |
| 0.1.4 | `mwfl::ipc`: bounded local Named Pipe frames | `ipc_framed` |
| 0.1.5 | `mwfl::diagnostics`: structured redaction and bounded sinks | `diagnostics_pipeline` |
| 0.1.6 | `mwfl::security`: current-user DPAPI and cleared plaintext | `security_dpapi` |
| 0.1.7 | `mwfl::deployment`: package identity and restart registration | `deployment_restart` |
| 0.1.8 | cross-component controller/worker stack | `foundation_stack` |
| 0.1.9 | combined acquisition and compatibility smoke | `foundation_overview` |

All examples support `--self-test`. System mutation remains explicit: the
Service example never installs itself, IPC names are process-unique, DPAPI uses
the current user, diagnostics uses a bounded temporary file, and deployment
does not download or install software.
