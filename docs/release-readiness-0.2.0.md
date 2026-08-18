# 0.2.0 release readiness

## Outcome

Publish `v0.2.0` as the next public-preview compatibility baseline and move all
first-party organization applications from an unreleased commit pin to the
immutable `v0.2.0` tag.

## Required gates

- `project(mwfl VERSION 0.2.0)`, templates, website, package defaults, release
  notes, and metadata verifiers agree on `v0.2.0`.
- VS2026 x64 Debug and Release, all optional components, installed-package
  consumers, independent public headers, examples, and side-effect-free
  self-tests pass.
- VS2022 x64 compatibility, VS2022/VS2026 ARM64 compilation, AddressSanitizer,
  coverage, static analysis, and reliability-repeat jobs pass.
- The release workflow rebuilds and tests the tagged commit before producing
  exactly two x64 ZIP archives and `SHA256SUMS-x64.txt`.
- The GitHub Release tag and project version match, every uploaded archive is
  non-empty, and the published checksums match the assets.
- All nine standalone applications configure, build, test, and pass GitHub
  Actions with their MWFL dependency pinned to `v0.2.0`.

## Explicit boundaries

- Downloadable binaries target VS2026/MSVC x64; ARM64 remains source/CI only.
- Ordinary CI does not imply that privileged SCM, Event Log source, Credential
  Manager, Task Scheduler, or update-handoff integration modes ran. Those
  mutations remain explicitly authorized local gates with unique identities
  and cleanup audits.
- The release provides update handoff primitives, not downloading, channel
  selection, update policy, or UI.
- ABI compatibility is not promised; consumers rebuild with a consistent MSVC
  toolset and runtime configuration.

## Release sequence

1. Commit the release candidate to `main` and require a successful CI run.
2. Create annotated tag `v0.2.0` from that exact commit and push it.
3. Require the tag-triggered Release workflow and published asset audit.
4. Update all organization applications to `GIT_TAG v0.2.0`, run their local
   Release tests, push `main`, and require all application CI runs.
5. Audit repository status, remote synchronization, release metadata, assets,
   checksums, and application pins before declaring the release complete.
