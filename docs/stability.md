# API stability policy

The `v0.1.0` public-preview release establishes the supported public baseline.
The audited baseline is recorded in [public-api-contract-audit.md](public-api-contract-audit.md).

From `v0.1.0` onward, documented stable APIs preserve source compatibility within the
current minor line. ABI compatibility is not promised: applications should
rebuild mwfl and their code with the same MSVC toolset and runtime configuration.

## Stability levels

- **Stable:** application lifetime, windows, typed events, native control
  ownership, DIP geometry, retained layout, message pumping, wakeups, checked
  operations, packaging, and raw Win32 escape hatches.
- **Provisional:** the command model and optional appearance helpers.
- **Example:** example application code is not library API.
- **Internal:** `mwfl::detail`, `src/detail`, and test-only definitions.

After `v0.1.0`, public removals or signature changes require release notes and a
compile-time compatibility fixture. Deprecations remain for at least one minor
release. The project does not silently change ownership, thread-affinity,
exception, or failure semantics.

The maintained configurations are 64-bit Windows 10 1809 or newer on x64
and ARM64 using the Microsoft Visual C++ compiler. Each is enforced in CI.
