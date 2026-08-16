## mwfl unreleased (next after 0.1.3)

Notes for changes on `main` that are not yet in a tagged release. They move
into the versioned release notes when the next version is cut.

### Public API

- `mwfl/message_pump.h` gains `mwfl::MessageLoop` and `mwfl::MessageFilter`;
  every public signature is now WTL-free. **Source-breaking for custom pumps:**
  `MessagePump::Run(WTL::CMessageLoop&)` became `Run(MessageLoop&)`, and the
  pump calls `loop.PreTranslate(message)` instead of
  `wtl_loop.PreTranslateMessage(&message)`. First-party code, templates, and
  documented examples needed no change.
- `WaitAwareMessagePump` and `MessageLoop` are non-copyable and non-movable
  (compile-time enforced by `tests/header_message_pump.cpp`).
- `Window::SetLayout` throws `mwfl::Error` instead of `std::runtime_error`
  when the owned layout cannot be arranged.
- `mwfl/application.h`, `mwfl/message_pump.h`, and `mwfl/detail/window_support.h`
  no longer include ATL or WIL. `mwfl::ui` still propagates the WIL and WTL
  include directories, so consumers that use WIL include `<wil/resource.h>`
  themselves (see "Third-party headers available to consumers" in
  `docs/api.md`).

### Fixes

- Message filters run newest-first again, as they did under WTL. Between the
  Phase 0 loop rewrite and this fix, a modeless dialog or property sheet
  created after the main window lost Escape/Enter/Tab keystrokes to the main
  window's accelerator table (`mwfl.message_loop.order`).
- A filter that unregisters itself during `PreTranslateMessage` no longer
  causes the next filter to be skipped for that message; duplicate
  `AddFilter` calls register once.
- Running `mwfl::Application` more than once in a process no longer crashes.
  The WTL module is initialized once, held per run, and terminated at process
  exit instead of being terminated (and left un-reinitializable) after every
  run (`mwfl.message_loop.twice`; Debug crashed at window creation, Release
  inside `CAppModule::Term`).
- `WM_NCDESTROY` member cleanup runs before base dispatch so an overridden
  `OnFinalMessage` that destroys the window object cannot trigger a
  use-after-free.
- The `docs/api.md` `WaitAwareMessagePump` example now lists designated
  initializers in declaration order and compiles.
- `scripts/verify.ps1` detects a preset build directory whose CMake cache was
  generated for a different checkout path (a moved repository) and
  reconfigures it instead of failing with a CMakeCache mismatch.

### Tests

- New `mwfl.message_loop.{unit,order,twice}` covering loop activation and
  nesting, filter-chain mutation during dispatch, `MessageLoop::Run` exit
  codes, `WaitAwareMessagePump` signals/idle, accelerator filter lifetime,
  modeless-dialog filter precedence, and repeated `Application` runs.
