# WTL independence plan

mwfl currently builds its window engine on WTL/ATL. This document records why
that dependency must leave the public API, what WTL actually provides today,
and the staged plan for removing it without destabilizing the released
surface. Phase 0 is implemented; Phases 1 and 2 are design commitments.

## Why

- **Public-ABI contamination.** `Window<T>` inherits `WTL::CFrameWindowImpl`,
  so every consumer compiles ATL headers it never asked for.
- **The `_Module` global.** WTL's convention is that the *application* defines
  `CAppModule _Module`. mwfl defining it inside the library makes any consumer
  that also uses WTL collide on a duplicate symbol at link time. Phase 0
  removed it from the public headers; Phase 1 removes it entirely.
- **Supply chain.** WTL is fetched from a SourceForge git remote with no
  content hash and no mirror; it is effectively in maintenance mode.
- **Fighting the base class.** mwfl already intercepts `WM_DESTROY` to defeat
  `CFrameWindowImpl`'s unconditional `PostQuitMessage`, implements its own
  accelerator filter instead of the frame's, and uses none of the frame's
  toolbar/status-bar machinery. The library pays for a frame window and uses
  only the thunk and `Create`.

## What WTL actually provides

Measured against the 0.1.3 tree, the consumed surface is three features:

| WTL feature | mwfl use | Replacement |
|---|---|---|
| `CAppModule` loop map | per-thread loop lookup for filters | `mwfl::MessageLoop::Current()` (done, Phase 0) |
| `CMessageLoop` + `CMessageFilter` | pump + pre-translate chain (accelerators, modeless dialogs, property sheets) | `mwfl::MessageLoop` / `mwfl::MessageFilter` (done, Phase 0) |
| `CFrameWindowImpl` + `CFrameWndClassInfo` | class registration, HWND→object thunk, `Create` | `detail::WindowCore` (Phase 1) |

Controls, layout, docking, splitters, dialogs, and property sheets are already
raw Win32. Examples, templates, tests, and documentation contain no direct WTL
use.

## Phase 0 — WTL-free public interfaces (implemented)

- `mwfl::MessageFilter` replaces `WTL::CMessageFilter` in every filter site
  (accelerators, `Dialog`, `PropertySheet`).
- `mwfl::MessageLoop` owns the pump and the pre-translate chain.
  `Application` activates its loop on the running thread;
  `MessageLoop::Current()` replaces `_Module.GetMessageLoop()`. Activation is
  an explicit stack so nested runs restore the previous loop.
- `MessagePump::Run(WTL::CMessageLoop&)` became `Run(MessageLoop&)`.
- `extern WTL::CAppModule _Module` left the public headers;
  `application.h`, `message_pump.h`, and `detail/window_support.h` no longer
  include ATL. `window.h` still includes ATL for its base class only.
- Behavior is preserved: idle handlers were never used, `GetMessage` semantics
  (including the `-1` continue) match the WTL loop, filters run newest-first
  exactly as `CMessageLoop::PreTranslateMessage` iterated its array backwards
  (a modeless dialog registered after the main window sees keystrokes before
  the accelerator filter — `mwfl.message_loop.order` proves it), and the
  `MWFL_TEST_FAIL_LOOP_REGISTRATION` injection point and lifecycle counters
  are unchanged. Unlike WTL, a filter that unregisters itself or others during
  dispatch is handled deterministically (each filter runs at most once per
  message) and duplicate registration is a no-op.
- The `_Module` lifetime is now process-wide (`src/detail/module.h`):
  initialized on the first `Application` run, reference-held per run, and
  terminated once at process exit. ATL's `CAtlModule::Term` destroys module
  critical sections that a later `Init` never recreates, so the previous
  per-run `Init`/`Term` pairing crashed the second `Application` in a process
  (Debug at window creation, Release inside `CAppModule::Term`).
  `mwfl.message_loop.twice` guards this until Phase 1 deletes `_Module`.

Source-compatibility note: authors of custom `MessagePump` implementations
must update the `Run` signature. No first-party or template code required
changes; the WTL loop type appeared in no example, test, or document. See
`docs/releases/unreleased.md`.

## Phase 1 — replace the engine

- `detail::WindowCore` registers window classes from the existing
  `ClassTraits`, creates HWNDs with `CreateWindowExW(..., lpParam = this)`,
  and binds the object in `WM_NCCREATE` via `GWLP_USERDATA` — no runtime
  thunk. `Window<T>` keeps its public shape (`GetHwnd`, `SetTitle`,
  `SetLayout`, typed handlers, `SafeWindowProc` exception containment) on top
  of `WindowCore` instead of `CFrameWindowImpl`.
- `_Module`, `CAppModule::Init/Term`, and the WTL fetch in
  `cmake/Dependencies.cmake` are deleted.
- Behavioral parity checklist: `WM_CREATE` failure returns −1 and aborts
  creation; `WM_NCDESTROY` detaches state exactly once; accelerator
  translation order is unchanged; `rcDefault` geometry math is preserved.

Exit gate: the full test matrix (lifecycle including all injected failures,
GUI self-tests, docking/document stress, ASan, `--repeat until-fail`) passes
on VS2026 x64 Debug/Release and ARM64 with no WTL sources on disk.

## Phase 2 — interop and contract cleanup

- `docs/design.md`'s "direct WTL integration" extension promise is replaced
  by the raw Win32 escape hatch, following `docs/stability.md`.
- If existing consumers need it, an optional `mwfl::wtl_interop` component can
  adapt a WTL message map into an `mwfl::MessageFilter`; it is not built or
  installed by default.

## Explicitly out of scope

Replacing ATL-style member names (`m_hWnd`, `m_hAccel`) is cosmetic and
deferred until Phase 1 forces the choice. No change to the documented
threading, ownership, or exception-containment contracts is planned in any
phase.
