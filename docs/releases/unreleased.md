## mwfl unreleased (next after 0.1.9)

This development baseline refines the provisional Foundation APIs introduced in
0.1.9. It does not assign a release version or tag yet.

- Unified blocking Foundation operations around one monotonic deadline,
  `std::stop_token`, and structured timeout/cancel/disconnect outcomes.
- Expanded Process with environment control, redirected stdin/stdout/stderr,
  bounded concurrent output collection, Job Object limits, process groups, and
  suspended-launch supervision.
- Reworked local Named Pipe IPC with pre-created reusable listeners, overlapped
  cancellation, current-user ACLs, remote-client rejection, peer identity, and
  scoped impersonation.
- Added shared console/SCM Service callbacks plus explicit, idempotent service
  query/install/update/start/control/stop/remove management.
- Added diagnostic fanout reports, pre-sink redaction, rotation, TraceLogging,
  explicit Event Log source management, and opt-in minidumps.
- Added fixed-allocation secure buffers, Credential Manager, token identity, SID
  and integrity queries, and focused security descriptor construction.
- Added recovery registration, package/file version identity, Authenticode
  policy, verified update handoff with rollback, and the independent
  `mwfl::scheduler` component.
- Added focused Foundation examples and the multi-file `supervised_worker`
  reference application with normal, cancel/timeout, crash, hang, malformed,
  disconnect, log-failure, and unauthorized evidence.

See `docs/foundation-migration-next.md` for source migration from 0.1.9.
