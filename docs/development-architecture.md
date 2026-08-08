# Development architecture

This is the change-oriented map for maintainers and coding agents. Public API
semantics remain in `api.md`, `reference.md`, and `stability.md`.

## Application and window lifecycle

`application.h/.cpp` owns process/module/COM setup and the outer exception
boundary. `window.h` owns typed dispatch, top-level HWND attachment, wake state,
and retained layout. `message_pump.*`, `wakeup.h`, and `timer.*` extend this
lifecycle. Changes here require lifecycle and advanced-lifecycle tests; never
allow an exception to escape native dispatch.

## Controls and events

Core controls live in `controls.*`; specialized families are split across
`navigation_controls.*`, `input_controls.*`, and `command_controls.*`.
`control_host.h` is the checked construction path and `events.h` is the public
typed event vocabulary. Add a wrapper only when ownership, creation failure,
thread affinity, preferred size, notifications, and native access are defined.

## Layout and DPI

`layout.*` contains the retained row/column/overlay tree. `dpi.*` owns DIP/pixel
conversion and per-window context. Layout stores native identities rather than
C++ wrapper ownership. Algorithm changes need deterministic unit/property cases
and DPI-aware examples.

## Commands, binding, and desktop integration

`command.*` is the shared action model. `binding.h` keeps model/control transfer
explicit. `desktop.*`, `appearance.*`, and `control_resources.*` wrap native
shell, composition, accessibility, and resource APIs. Preserve cancellation vs
failure and document native-handle ownership.

## Adding a public API

1. Choose the smallest owning public header.
2. Define ownership, threading, failure, and compatibility status.
3. Implement without leaking internal types.
4. Add an independent-header compile test.
5. Add runtime behavior tests where observable.
6. Update `docs/reference.md` and focused API documentation.
7. Add or update one canonical example and its metadata.
8. Run `./scripts/verify.ps1 -Mode Full`.

## Adding a control

Add the wrapper and options beside its control family, implement native
creation and measurement, route notifications through typed events, instantiate
it in the appropriate gallery, update accessibility coverage, and keep the
component/site catalogs synchronized.

## Adding a typed event

Define the public event contract in `events.h`, decode raw message data at the
window dispatch boundary, preserve default propagation, add decode/matching and
lifecycle tests, then demonstrate the event in the narrowest example.

