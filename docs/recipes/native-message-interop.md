# Add native message interop

Prefer typed handlers already provided by `WindowBase`. For an application-defined
message, choose an ID at or above `WM_APP`, post or send it through `GetHwnd()`, and
handle it in `OnMessage(const mwfl::WindowMessage&)`.

- Return `Handled(result)` only when the message was consumed.
- Return `Propagate()` for every other message.
- Keep payload ownership valid for the delivery mechanism; never post a pointer to
  stack storage.
- Catch exceptions before returning to Win32.

Canonical compiled code: `agent-evals/fixtures/native_message.cpp` and
`examples/native_message`.
