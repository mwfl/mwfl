# API stability policy

The `v0.1.0` official release establishes the supported public baseline.
The audited baseline is recorded in [public-api-contract-audit.md](public-api-contract-audit.md).

From `v0.1.0` onward, documented stable APIs preserve source compatibility within the
0.1.x line. ABI compatibility is not promised: applications should
rebuild mwfl and their code with the same MSVC toolset and runtime configuration.

During the 0.x public preview, patch releases may add backward-compatible
components and public APIs. Existing stable signatures are not removed or
changed within the current minor line. A minor-version increment is reserved for a
deliberate provisional-API compatibility break or a new compatibility baseline.

## Stability levels

- **Stable:** application lifetime, windows, typed events, native control
  ownership, DIP geometry, retained layout, message pumping, wakeups, checked
  operations, packaging, and raw Win32 escape hatches.
- **Provisional:** the command model and optional appearance helpers.
- **Stable Foundation baseline:** `core`, Service, process, IPC, diagnostics,
  security, deployment, and scheduler APIs released in 0.1.0. Their ownership,
  cancellation, security, failure, and explicit-mutation semantics are
  contractual within the 0.1.x line.
- **Example:** example application code is not library API.
- **Internal:** `mwfl::detail`, `src/detail`, and test-only definitions.

After `v0.1.0`, public removals or signature changes require release notes and a
compile-time compatibility fixture. Deprecations remain available for at least
one subsequent minor release and carry a documented replacement before
removal. The project does not silently change ownership, thread-affinity,
exception, or failure semantics.

The complete installed-header baseline is machine-readable in
[`public-api-baseline-0.1.txt`](public-api-baseline-0.1.txt). CI verifies that
every baseline header remains present, while the frozen package consumer
continues to compile through installed-package, `FetchContent`, and
`add_subdirectory` acquisition. Additive headers and APIs are allowed.

The maintained configurations are 64-bit Windows 10 1809 or newer on x64
and ARM64 using the Microsoft Visual C++ compiler. Each is enforced in CI.
