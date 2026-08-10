# Modern Win32++ coverage plan

## Objective

Complete the remaining useful Win32++ application surfaces without copying its
API hierarchy or historical compatibility costs. Every addition targets
Windows 10+, C++20, Visual Studio 2026, MSVC, and x64 first. The result should
let an application express the same native Windows behavior with less state,
clearer ownership, typed failures, and better coding-Agent evidence.

The audited baseline is [`win32xx-comparison.md`](win32xx-comparison.md). Major
application categories are already covered through 0.8. This plan closes the
remaining direct API gaps while retaining standard-library composition where a
UI framework wrapper would add no value.

## Product principles

1. Measure scenario coverage, not class-count parity.
2. Add a wrapper only when it materially improves ownership, lifetime,
   threading, encoding, DPI, accessibility, or failure handling.
3. Keep raw HWND, HDC, COM, and native-message escape hatches available.
4. Prefer values, RAII, `std::span`, `std::ranges`, `std::chrono`, concepts,
   `std::filesystem`, and explicit result types over framework replacements.
5. Keep optional native ecosystems out of `mwtl::mwtl`.
6. Ship every public feature as API, example, recipe/tutorial, package evidence,
   Agent metadata, Pages component entry, and automated tests.

## Phase 1 — native text and scrolling composition

- Add a Unicode RichEdit host with version discovery, explicit text/selection
  ranges, streamed load/save, formatting runs, typed notifications, URL and
  IME behavior, and borrowed native interfaces.
- Add a DPI-aware `ScrollView` composition model for arbitrary child HWNDs,
  keyboard/wheel/pan routing, logical extents, focus visibility, and accessible
  scroll state.
- Build a rich-text editor and a large scrollable inspector reference.

**Exit gate:** applications can reproduce Win32++ RichEdit and ScrollView
samples without raw lifetime plumbing; malformed ranges, very large content,
reentrancy, DPI changes, and teardown orders have executable coverage.

## Phase 2 — focused GDI ownership

- Introduce move-only, adopt/borrow-explicit wrappers for bitmap, brush, pen,
  font, region, palette, and compatible/memory DC transactions.
- Encode select/restore invariants so an owned GDI object cannot be destroyed
  while selected into a managed DC.
- Keep drawing operations thin and interoperable with raw GDI; do not build a
  second rendering framework beside Direct2D.

**Exit gate:** the representative Win32++ GDI/bitmap/metafile scenarios have
balanced success and failure paths, handle-budget stress tests, and direct raw
HDC composition examples.

## Phase 3 — remaining common dialogs

- Add structured color and font dialog results with owner HWND, cancellation,
  custom colors, LOGFONT conversion, DPI-aware preview guidance, and hook
  exception containment.
- Evaluate a focused Find/Replace dialog host extracted from the Notepad
  reference so modeless ownership and registered-message routing are reusable.

**Exit gate:** cancellation is not an error, hooks cannot leak exceptions,
owners and buffers outlive the native dialog, and all workflows have offline
GUI self-tests.

## Phase 4 — native hosting decisions

- Document and test the generic `NativeHost` path for uncommon controls.
- Keep WebView2 as the web-content default. Add a generic ActiveX container only
  if two real non-browser controls demonstrate a reusable need; otherwise retain
  raw COM hosting as an explicit non-goal.
- Publish migration recipes for Win32++ `CAXHost`/`CWebBrowser`, sockets, file
  search, archives, synchronization, threads, time, and strings.

**Exit gate:** every remaining Win32++ class family has either a tested direct
surface, a compiled composition recipe, or an explicit evidence-backed non-goal.

## Phase 5 — ecosystem and replacement proof

- Build migration guides and compile fixtures for representative Win32++ SDI,
  MDI, docking, Ribbon, printing, drawing, browser, editor, and utility samples.
- Complete a mini-IDE reference combining document workspace, docking,
  Scintilla, output/log RichEdit, commands, settings, printing, and Shell.
- Measure source size, build time, dispatch overhead, handle deltas, startup,
  shutdown, DPI, keyboard, accessibility, and Agent task success.

**Exit gate:** for each representative Win32++ application category, a newcomer
and a coding Agent can find one canonical mwtl path, build it with documented
VS2026 x64 commands, and pass deterministic tests without internal APIs.

## Definition of done for every slice

- Public headers are independently compilable and do not expose `detail`.
- Ownership, borrowing, thread/apartment affinity, reentrancy, units, encoding,
  native escape hatches, failure states, and teardown order are documented.
- Unit, property/quality, native lifecycle, GUI self-test, stress/resource, and
  package-consumer coverage are added as applicable in Debug and Release.
- A complete example, beginner path, focused recipe, API index entry,
  capabilities entry, Agent fixture, and Pages catalog card agree on symbols.
- The milestone reflection identifies defects found during completion and
  answers whether the slice is thoroughly complete before the next begins.

## Validation sequence

The primary feature gate remains local Visual Studio 2026, MSVC C++20, x64,
Debug and Release. VS2022, ARM64, clang-cl, sanitizers, coverage, static
analysis, Pipeline, and GitHub Actions are repaired in a separate compatibility
phase after feature slices stabilize; they are not used to dilute or redesign
the modern C++20 API.
