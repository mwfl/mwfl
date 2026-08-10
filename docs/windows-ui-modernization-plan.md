# Modern Windows UI API coverage plan

This is the post-`0.1.0` capability plan. The active short-term gate is the
[first public preview readiness plan](release-readiness.md); no phase below
starts until that release gate is complete.

## Objective

Make useful Windows UI and desktop APIs available through a concise, modern
C++20 development model. mwtl should remove repetitive setup and unsafe
lifetime plumbing while preserving native behavior, native handles, messages,
styles, return values, and composition with the wider Windows SDK.

Every addition targets Windows 10+, C++20, Visual Studio 2026, MSVC, and x64
first. The desired result is application code with fewer hidden states, clear
ownership, typed events, structured failures, predictable teardown, and enough
machine-readable evidence for a coding Agent to choose the correct API.

## Product principles

1. Organize the library around developer tasks and complete applications, not
   a class-count target.
2. Add a wrapper only when it materially improves ownership, lifetime,
   threading, encoding, DPI, accessibility, failure handling, or composition.
3. Keep raw HWND, HDC, COM, and native-message escape hatches available.
4. Prefer values, RAII, `std::span`, `std::ranges`, `std::chrono`, concepts,
   `std::filesystem`, and explicit result types over framework-specific utility
   types.
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

**Exit gate:** both scenarios work without raw lifetime plumbing; malformed
ranges, very large content, reentrancy, DPI changes, keyboard/accessibility,
and teardown orders have executable coverage.

## Phase 2 — focused GDI ownership

- Introduce move-only, adopt/borrow-explicit wrappers for bitmap, brush, pen,
  font, region, palette, and compatible/memory DC transactions.
- Encode select/restore invariants so an owned GDI object cannot be destroyed
  while selected into a managed DC.
- Keep drawing operations thin and interoperable with raw GDI; do not build a
  second rendering framework beside Direct2D.

**Exit gate:** representative bitmap, paint, memory-DC, region, font, palette,
and metafile workflows have balanced success/failure paths, handle-budget
stress tests, and direct raw-HDC composition examples.

## Phase 3 — remaining common dialogs

- Add structured color and font dialog results with owner HWND, cancellation,
  custom colors, LOGFONT conversion, DPI-aware preview guidance, and hook
  exception containment.
- Extract a focused Find/Replace dialog host from the Notepad reference so
  modeless ownership and registered-message routing are reusable.

**Exit gate:** cancellation is not an error, hooks cannot leak exceptions,
owners and buffers outlive the native dialog, and all workflows have
deterministic offline GUI self-tests.

## Phase 4 — native hosting and composition guidance

- Document and test the generic `NativeHost` path for uncommon controls.
- Keep WebView2 as the web-content default. Add a generic ActiveX container only
  if two real non-browser controls demonstrate a reusable ownership contract;
  otherwise retain raw COM hosting as an explicit composition path.
- Publish compiled recipes for Windows APIs that should remain outside a UI
  layer, including sockets, file enumeration, serialization, synchronization,
  worker threads, and time-sensitive background work.

**Exit gate:** every targeted Windows UI scenario has either a tested direct
surface, a compiled composition recipe, or an explicit evidence-backed
boundary explaining why ordinary C++20 or Windows SDK APIs are the better path.

## Phase 5 — ecosystem and usability proof

- Complete a mini-IDE reference combining document workspace, docking,
  Scintilla, output/log RichEdit, commands, settings, printing, and Shell.
- Add substantial reference applications for the new text, scrolling, GDI, and
  common-dialog surfaces instead of validating them only in isolated tests.
- Measure source size, build time, dispatch overhead, handle deltas, startup,
  shutdown, DPI, keyboard, accessibility, and coding-Agent task success.
- Make the Components catalog the authoritative task-to-symbol discovery path
  for every implemented feature and documented native composition boundary.

**Exit gate:** a newcomer and a coding Agent can start from a Windows desktop
task, find one canonical C++20 path, build it with documented VS2026 x64
commands, and pass deterministic tests without depending on internal APIs.

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
Debug and Release. VS2022, ARM64, sanitizers, coverage, static
analysis, Pipeline, and GitHub Actions are repaired in a separate compatibility
phase after feature slices stabilize; they are not used to dilute or redesign
the modern C++20 API.
