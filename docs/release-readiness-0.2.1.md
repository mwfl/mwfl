# 0.2.1 release readiness

## Outcome

Publish `v0.2.1` as the patch release that adds the application facade and
shared update checker, hardens Foundation waits and process output collection,
and moves all nine organization applications to the immutable tag.

## Required gates

- Project version, templates, website, package defaults, release notes, and
  metadata verifiers agree on `v0.2.1`.
- VS2026 x64 Debug and Release builds pass every configured test, public-header
  probe, package consumer, example, and side-effect-free self-test.
- The tag workflow builds, tests, packages, checksums, and attests x64 and ARM64.
- All eight public applications and private Rho PDF build against `v0.2.1` and
  use `mwfl::app_support::UpdateChecker` without copied implementations.
- Every public application release workflow continues to produce a Portable ZIP.

## Explicit boundaries

- The supported compiler is MSVC; this release does not add a Clang promise.
- System mutation remains behind explicit integration gates.
- Update checking does not download or replace binaries.
- Private application names and links are not added to public catalogs.

## Release sequence

1. Pass local VS2026 x64 Debug and Release gates.
2. Commit and push the release candidate to `main`; require successful CI.
3. Create and push annotated tag `v0.2.1`; audit the Release workflow and assets.
4. Pin all nine applications to `v0.2.1`, build locally where dependencies are
   available, push every `main`, and audit their Actions runs.
5. Confirm clean, synchronized repositories and exact dependency pins.
