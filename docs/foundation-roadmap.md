# Modern Windows Foundation Layer roadmap

## Product decision

mwfl is a native Windows application foundation rather than only a UI layer.
It supports three first-class hosts:

- GUI applications link `mwfl::ui`;
- Windows Services opt into `mwfl::service`;
- console and command-line applications use dependency-light foundation
  components without acquiring HWND or GUI startup requirements.

Visual Studio 2026/MSVC C++20 and x64 are the first implementation and release
gate. Windows 10 or newer remains the operating-system baseline. Public designs
must preserve ARM64 compatibility, while the first foundation slices may ship
after complete x64 evidence and before the full ARM64 matrix is promoted to a
release gate. Visual Studio 2022 compatibility is best effort during this
expansion and is not allowed to constrain the initial C++20 API design.

## Component boundary

| Target | Role | Acquisition |
|---|---|---|
| `mwfl::core` | native error values, handles, wait results, cancellation, Unicode boundaries, diagnostic event values | shared dependency of foundation components |
| `mwfl::ui` | current HWND application, controls, events, layout, documents, and desktop workflows | explicit UI target |
| `mwfl::service` | single-service host, typed controls and state, console debug host, SCM management | opt-in |
| `mwfl::process` | child processes, redirected pipes, environment, Job Objects, process-tree lifetime | opt-in |
| `mwfl::ipc` | local named-pipe client/server and bounded framing | opt-in |
| `mwfl::diagnostics` | Event Log, ETW, debug/file sinks, crash and minidump policy | opt-in |
| `mwfl::security` | DPAPI, Credential Manager, token/SID queries, bounded security descriptors | opt-in |
| `mwfl::deployment` | restart/recovery, package identity, version/signature and update handoff helpers | opt-in |

Existing rendering, printing, OLE, Shell, Ribbon, MDI, graphics, WebView2, and
Scintilla targets remain opt-in. `mwfl::ui` never links Service Control Manager,
process supervision, IPC, diagnostic sinks, credentials, or deployment policy.

## Delivery order

### F0 — UI target migration and contracts

- Make `mwfl::ui` canonical in build-tree, installed, FetchContent, template,
  example, documentation, metadata, and package-consumer paths.
- Keep `mwfl::mwfl` as a compatibility target for the 0.1 line.
- Record GUI, Service, and CLI host contracts and component boundaries.

Exit gate: new and compatibility targets both configure from an installed
package; all first-party examples link the canonical target; no dependency or
startup behavior changes.

### F1 — shared foundation core

- Add move-only native handle ownership with explicit adopt/borrow operations.
- Add structured wait, timeout, abandoned, cancellation, and native failure
  results that compose with `std::stop_token`.
- Unify Win32 and HRESULT error capture without discarding native codes.
- Keep UI message pumping separate while permitting wait-result composition.

Exit gate: independent headers, x64 Debug/Release package consumers, handle
budget stress, cancellation races, and no exception across native callbacks.

### F2 — Windows Service MVP

- Support one `SERVICE_WIN32_OWN_PROCESS` service per executable.
- Map start, stop, shutdown, pause, continue, power, session, and custom
  controls to typed, exception-contained callbacks.
- Drive cooperative shutdown through `std::stop_token` and explicit service
  status transitions, checkpoints, wait hints, Win32 exit code, and
  service-specific exit code.
- Provide a console debug host using the same application callback and Ctrl+C
  cancellation.
- Provide explicit query/install/update/uninstall/control operations; never
  self-install implicitly and never retain plaintext credentials.

Exit gate: pure state-machine tests require no elevation; an isolated,
uniquely-named x64 integration service is installed, started, controlled,
stopped, and removed on an approved development machine; cleanup is verified
after success and injected failures.

### F3 — process and Job Object supervision

- Add safe executable/argument/environment construction and redirected pipes.
- Distinguish graceful request, timeout, cancellation, exit, and forced
  termination.
- Manage process trees with Job Objects and explicit kill-on-close policy.

Exit gate: bounded output, inherited-handle allowlist, child/grandchild cleanup,
timeout races, resource accounting, and GUI/Service/CLI consumers.

### F4 — local named-pipe IPC

- Add byte-stream and bounded length-framed modes with overlapped I/O.
- Support cancellation, timeout, disconnect, reconnect, identity queries, and
  explicit impersonation.
- Default examples to current-user/current-session access rather than broad
  machine access.

Exit gate: a GUI controller communicates with a Service, which supervises a
worker process; malformed frames, hostile sizes, disconnects, shutdown orders,
and ACL boundaries have executable evidence.

### F5 — diagnostics and security

- Add structured events with Debug Output, Event Log, ETW, and bounded file
  sinks plus correlation IDs and explicit sensitive-field handling.
- Add opt-in minidump policy without replacing application crash ownership.
- Add DPAPI, Credential Manager, token/SID, elevation, and focused security
  descriptor helpers.

Exit gate: secrets are never logged, secret buffers are cleared, callbacks are
contained, registration is reversible, dumps/logs are bounded, and tests leave
no machine-wide state.

### F6 — scheduled work and deployment lifecycle

- Add focused Task Scheduler composition for tasks that should not be Services.
- Add Application Restart and Recovery, package-identity discovery, version and
  signature verification, and safe update handoff primitives.
- Do not add a downloader, package manager, or generic updater policy.

## Explicit exclusions

mwfl does not implement HTTP/TLS, a socket framework, databases or ORM,
serialization formats, a general coroutine runtime or thread pool, compression,
cryptographic algorithms, cloud SDKs, general RPC, drivers, or arbitrary Shell
extensions. Standard C++ and focused third-party libraries remain the normal
composition path.

## Definition of done for every foundation slice

- Public headers independently compile and expose no `detail` types.
- Ownership, borrowing, thread/apartment affinity, callback/reentrancy,
  cancellation, security, elevation, failure, and teardown are documented.
- Native mutations use unique test identities and are reversible; privileged
  tests clean up in success, failure, cancellation, and interrupted runs.
- Model, native lifecycle, stress/resource, package, example, documentation,
  capability metadata, and Agent evidence agree.
- Visual Studio 2026/MSVC C++20 x64 Debug and Release are the first mandatory
  gate. Every intentionally deferred VS2022 or ARM64 check is reported.
