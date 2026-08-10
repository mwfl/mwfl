# Common mistakes when generating mwfl applications

## Updating controls from a worker

Wrong: a worker calls a UI-thread-affine wrapper.

```cpp
std::jthread worker_{[this] { status_.SetText(L"Done"); }};
```

Correct: capture a `WindowWakeup`; update controls in `OnWakeup()` on the UI
thread. See `examples/wakeup/main.cpp` and the
[background work recipe](recipes/background-work.md).

## Giving controls local lifetime

Wrong: create a local `Button` in `BuildUI()`. Its destructor destroys the HWND
when `BuildUI()` returns. Store controls as window members instead. `ControlHost`
does not take C++ ownership of wrapper objects.

## Treating every event as handled

Returning `Handled()` suppresses the native/default path. Return `Propagate()`
when the event was not consumed, including DPI events that still need normal
window processing.

## Guessing framework APIs

mwfl has no signal/slot layer, virtual DOM, data-context object, message-map
macro requirement, or automatic background dispatcher. Verify every symbol in
`include/mwfl`, `docs/agent-reference.md`, or a compiled example.

## Mixing pixels and DIPs

Layout lengths use `Dip` values such as `24.0_dip`. Raw Win32 RECT coordinates
are pixels. Convert deliberately at the native boundary; do not pass arbitrary
pixel constants into retained layout.

## Losing asynchronous lifetime

Do not capture references to local variables in workers or posted callbacks.
Make the worker a window member, request stop during destruction through
`std::jthread`, and rely on `WindowWakeup::TryWake()` returning false after the
target window becomes invalid.

## Throwing through callbacks

No exception may cross a Win32 callback. Use checked helpers such as `Must`
during setup, preserve `noexcept` handlers where declared, and convert expected
runtime failures into explicit state or diagnostics.

## Using internal headers

Never include files from `src/` or name `mwfl::detail` types in application
code. They have no compatibility guarantee.

## Omitting the Windows application setup

Use a `WIN32` executable, `wWinMain`, Unicode source, C++20, and the supplied
Per-Monitor-V2 manifest. Copy a template instead of reconstructing this setup.

## Consuming a moving branch

Use a release tag or immutable commit in `FetchContent`. `main` is appropriate
only when explicitly testing unreleased mwfl changes.
