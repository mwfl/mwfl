# 0.3.0 release readiness

## Outcome

Publish `v0.3.0` with the opt-in privacy-conscious thread-information minidump
tier, and move maintained templates and consumer examples to the immutable tag.

## Required gates

- Project version, templates, website, package defaults, release notes, and
  metadata verifiers agree on `v0.3.0`.
- VS2026 x64 builds and all configured tests pass.
- The tag workflow builds, tests, packages, checksums, and attests x64 and ARM64.
- Maintained consumer examples pin `v0.3.0` and their CI remains green.

## Explicit boundaries

- `MiniDumpKind::Small` remains the default.
- Thread information is opt-in and does not enable broad process-memory capture.
- The supported compiler remains MSVC.

## Release sequence

1. Pass local verification and `main` CI.
2. Create and push annotated tag `v0.3.0`.
3. Audit the Release workflow and published assets.
4. Pin maintained consumers to `v0.3.0` and audit their Actions runs.
