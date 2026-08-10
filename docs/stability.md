# API stability policy

mwtl has not yet published its supported public baseline. Until the replacement
`v0.1.0` release is cut, every API is reviewable and may be renamed, simplified,
or removed without a compatibility shim. This one-time pre-public window exists
to make the first supported surface smaller and more coherent.

The replacement `v0.1.0` tag starts the compatibility policy below. From that
release onward, documented stable APIs preserve source compatibility within the
current minor line. ABI compatibility is not promised: applications should
rebuild mwtl and their code with the same MSVC toolset and runtime configuration.

## Stability levels

- **Stable:** application lifetime, windows, typed events, native control
  ownership, DIP geometry, retained layout, message pumping, wakeups, checked
  operations, packaging, and raw Win32 escape hatches.
- **Provisional:** the command model and optional appearance helpers.
- **Example:** example application code is not library API.
- **Internal:** `mwtl::detail`, `src/detail`, and test-only definitions.

After `v0.1.0`, public removals or signature changes require release notes and a
compile-time compatibility fixture. Deprecations remain for at least one minor
release. The project does not silently change ownership, thread-affinity,
exception, or failure semantics.

The maintained configurations are 64-bit Windows 10 1809 or newer on x64
and ARM64 using MSVC, plus clang-cl on x64. Each is enforced in CI.
