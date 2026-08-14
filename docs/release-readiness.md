# First public preview readiness plan (archived)

> Historical record: this plan describes the pre-release `v0.1.0` gate. The
> current public preview is `v0.1.3`; use the current release notes, README, and
> verification scripts for active release status.

## Outcome

Publish one clean `v0.1.0` as mwfl's first supported public preview. The old
release and tag were removed on 2026-08-10 because they had no known users. The
replacement release is created only after the current source, all 47 examples,
optional integrations, packages, documentation, and website pass the gates
below from the same commit.

Until that tag exists, there is intentionally no source-compatibility promise.
This is the last planned opportunity to remove or reshape public APIs without a
deprecation layer.

## Review findings

The repository-wide review covers every tracked public header and source file,
the CMake target graph, all example directories, tests, package metadata,
machine-readable Agent indexes, and Pages content. The architecture is already
coherent: real HWND ownership stays explicit, callbacks contain exceptions,
layout uses DIPs, optional SDKs remain isolated, and raw Windows interop remains
available.

The remaining work is hardening rather than another feature milestone:

1. **Public API consistency.** Review creation, ownership, failure, cancellation,
   thread/apartment, units, and native escape-hatch semantics by header family.
   Remove aliases or overloads only when they duplicate another canonical path.
   Prefer `ControlHost` for ordinary construction and structured result types
   where cancellation or partial completion cannot be represented by `bool`.
2. **No historical names in active tooling.** Presets and user-facing commands
   describe their purpose, not an old milestone. The all-integrations preset is
   `vs2026-x64-optional`; no compatibility alias is retained before release.
3. **One source of truth for discovery.** `docs/capabilities.json`,
   `docs/examples.json`, `docs/api-index.json`, recipes, tutorials, README, and
   the generated Components catalog must describe the same shipped surface.
4. **Portable generated text.** Generators must emit UTF-8 correctly when run
   from Windows PowerShell 5 or newer. ASCII source or HTML entities are used
   for fixed site punctuation where PowerShell source decoding would be
   ambiguous.
5. **Honest release state.** Versioned templates may target `v0.1.0`, but the
   website must not claim a tested package until the replacement tag, artifacts,
   checksums, and release page all resolve from the final commit.

## Release gates

### Gate A - surface freeze candidate

- Inspect every file under `include/mwfl` and its corresponding implementation.
- Classify every public type as stable, provisional, optional, or internal.
- Record ownership, borrowing, thread/apartment affinity, failure/cancellation,
  units, encoding, teardown, and native escape hatches.
- Resolve inconsistent names, redundant overloads, silent failure paths, and
  public exposure of implementation details now; do not add compatibility
  wrappers for an unpublished surface.
- Regenerate the compile-only public baseline fixture only from the final
  candidate commit. That fixture becomes binding after `v0.1.0` is published.

### Gate B - every example is real

- Configure and build all 41 core examples plus the optional WebView2,
  Scintilla, Markdown Editor, and PDF Viewer applications in VS2026, MSVC
  C++20, x64 Debug and Release.
- Run every registered unit, lifecycle, native integration, GUI self-test,
  stress/resource, metadata, documentation, and package-consumer test.
- Verify each `docs/examples.json` entry names an existing `main.cpp`, is added
  by `examples/CMakeLists.txt`, builds in its declared component configuration,
  and links from the public discovery surfaces.
- Launch the principal starter and reference applications for a final visual
  smoke pass: Hello, controls galleries, Notepad, document workspace, docking,
  graphics, printing, WebView2, and Scintilla.

### Gate C - documentation and Agent truth

- Reconcile API reference, beginner tutorial, build instructions, recipes,
  examples, capability metadata, API index, `llms.txt`, and `llms-full.txt`.
- Run the catalog generator twice and require byte-identical output.
- Reject mojibake, stale milestone preset names, broken local links, stale
  version pins, missing capabilities, and missing recipe/tutorial catalog links.
- Browser-test Pages at desktop and phone widths with search, filters, theme,
  keyboard navigation, preview dialogs, and zero console errors.
- Run the 34 coding-Agent eval tasks against public headers only and review
  failures for discovery ambiguity rather than teaching internal workarounds.

### Gate D - release reproducibility

- From a clean checkout of the candidate commit, run
  `./scripts/verify.ps1 -Mode Full -VisualStudio 2026 -Architecture x64`.
- Repeat the all-integrations Debug and Release builds with
  `vs2026-x64-optional-debug` and `vs2026-x64-optional-release`.
- Build and consume installed core, WebView2, and Scintilla packages in Debug
  and Release; verify licenses and checksums.
- Record exact test totals, Visual Studio/MSVC/SDK versions, artifact names,
  hashes, and intentionally deferred matrices.

### Gate E - publish and observe

- Merge the release candidate to `main`; require a clean worktree and matching
  `origin/main` commit.
- Create the annotated `v0.1.0` tag from that commit and publish one GitHub
  Release with the VS2026/MSVC x64 package and SHA-256 checksum file produced by the
  release workflow.
- Verify the release URL, archive downloads, FetchContent templates, installed
  package, GitHub Pages, changelog, and component catalog from outside the
  development checkout.
- Announce it as a public preview. Collect real usage before expanding the
  stable surface or starting the post-0.1 capability plan.

## Stop-ship conditions

Do not publish while any of these is true: a public header does not compile
independently; a declared example is not built; Debug or Release tests fail or
flap; optional examples are unverified; package consumption differs from source
consumption; documentation describes a nonexistent symbol; Pages contains a
broken link or mojibake; a callback can leak an exception; ownership or thread
affinity is ambiguous; or release artifacts do not reproduce the tested commit.

## Scope after `0.1.0`

After a short public-preview observation period, begin Phase 1 of
`windows-ui-modernization-plan.md`: RichEdit and scrolling composition. GDI
ownership, remaining common dialogs, native-hosting guidance, and ecosystem
proof follow only after each earlier phase passes its own executable exit gate.
Compatibility work for VS2022, ARM64, sanitizers, coverage, and hosted
CI remains a separate audit and must not force a weaker C++20 API.

## Candidate verification record

The 2026-08-10 local candidate audit uses Visual Studio 2026 18.8.12023.21,
MSVC 19.51.36252, Windows SDK 10.0.26100, C++20, and x64:

- Core Debug: 162/162 tests passed.
- Core Release: 162/162 tests passed.
- All optional integrations Debug: 175/175 tests passed, including the installed
  package consumer, parser mutation fuzz, and application-generator test.
- All optional integrations Release: 175/175 tests passed, including the same
  installed package and reliability coverage.
- Native source coverage: 82.74% (8,697/10,511 lines), above the 74% CI floor;
  all 159 tests included in the coverage run passed.
- Capability metadata: 45 discoverable capabilities, 45 compiled examples, and six independently published applications.
- The two environment-sensitive native tests found during the audit each pass
  five consecutive runs after their focus and process-cache assumptions were
  corrected.

The first binary release intentionally packages x64 only. ARM64 remains a
source target but is outside the `0.1.0` release artifact gate until its remote
packaging path is stabilized.

The final human product pass follows
[`release-manual-test.md`](release-manual-test.md). It covers package integrity,
Markdown multi-document editing and recovery, Notepad text/file behavior, PDF
tabs and embedded viewer workflows, Hot Corners across monitor topologies, DPI,
keyboard, accessibility, and an extended stability run.

This record proves the local automated candidate. The remaining stop-ship work
is the final browser/visual smoke pass, clean-checkout reproduction, release
artifact workflow, external download/FetchContent verification, and creation of
the replacement tag and GitHub Release from the merged commit.
