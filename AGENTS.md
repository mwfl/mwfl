# mwtl repository guide for coding agents

This file is the authoritative development contract for Codex, Claude Code,
and other coding agents working on mwtl itself. For generating applications
that consume mwtl, start with `docs/agent-usage.md` instead.

## Project constraints

- Windows-only, 64-bit x64 and ARM64.
- C++20 with an MSVC-compatible ABI.
- Supported IDE/toolchains: Visual Studio 2022 and Visual Studio 2026.
- Prefer Visual Studio 2026 for development; preserve Visual Studio 2022
  compatibility unless a project decision explicitly changes the minimum.
- The public library is a thin layer over real HWNDs, not a renderer or virtual
  UI framework.

## Repository map

| Path | Responsibility |
|---|---|
| `include/mwtl/` | Stable and provisional public C++ API |
| `src/` | Library implementation |
| `src/detail/` | Internal implementation; never application API |
| `tests/` | Unit, lifecycle, integration, package, and metadata tests |
| `examples/` | Compiled canonical behavior and usage examples |
| `templates/` | Copyable consumer projects |
| `docs/` | Design, contracts, recipes, and public reference |
| `site/` | Static GitHub Pages content |
| `cmake/` | Dependency and package configuration |
| `scripts/` | Developer environment and validation entry points |

Never edit generated content under `build/` or `.vs/`. Do not treat dependency
sources found below a build tree as repository code.

## Core invariants

- No exception may cross a Win32 callback.
- UI wrappers belong to their creating thread. Cross-thread work needs an
  explicit handoff such as `WindowWakeup`.
- Ownership of HWNDs and other native handles must remain explicit.
- Preserve the native Win32 escape hatch; do not add a closed abstraction layer.
- Layout values are DIPs; raw Win32 geometry is normally pixels.
- Do not expose `mwtl::detail` or `src/detail` in public signatures.
- Stable API changes follow `docs/stability.md`.

## Change rules

- Public header change: update or add an independent-header compile target,
  observable runtime tests, ownership/threading/failure documentation, and a
  canonical example when the usage pattern changes.
- New public source/header: add it to CMake install/export and API-surface
  verification.
- Control change: cover creation, lifetime, command/notification behavior,
  accessibility where relevant, and the appropriate gallery.
- Layout change: run unit, quality/property, DPI, and relevant example tests.
- Lifecycle/message-pump/wakeup change: run lifecycle and advanced lifecycle
  categories, including GUI self-test when applicable.
- CMake/package change: run package consumer in Debug and Release.
- Documentation/site/example catalog change: run the docs and metadata modes.
- Capability metadata change: run `./scripts/generate-site-components.ps1`,
  commit the generated `site/components/catalog.html`, and run the docs mode.
  The Pages verifier requires every capability, recipe, and tutorial to remain
  discoverable from the generated catalog.

See `docs/development-architecture.md` and `docs/change-matrix.json` for the
full path-to-validation map.
Use `docs/api-index.json` to map a user task to public symbols, examples, tests,
and constraints before searching implementation files.
Use `docs/scope-map.md` before adding a feature that resembles another Windows
UI framework; it defines direct support, raw Win32 composition, and exclusions.

## Standard workflow

```powershell
./scripts/doctor.ps1
./scripts/verify-change.ps1 -Execute
./scripts/verify.ps1 -Mode Fast
```

Before handoff, run the narrowest relevant additional mode. Use `-Mode Full`
for public API, lifecycle, packaging, or broad changes. Select a toolchain with
`-VisualStudio 2022` or `-VisualStudio 2026`; Auto prefers 2026.

Do not claim a check passed unless it was executed. Report the selected Visual
Studio version, architecture, configuration, and any intentionally skipped
checks.

## Editing style

- Match `.clang-format` and `.editorconfig` without reformatting unrelated code.
- Use `/W4`, `/permissive-`, `/EHsc`, `/utf-8`, and warnings-as-errors where the
  existing target does.
- Prefer small public surfaces, RAII, typed events, explicit failure, and
  source-located diagnostics.
- Preserve unrelated user changes in a dirty worktree.
