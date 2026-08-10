# `mwfl::Application` demo

This executable demonstrates the complete milestone-1 application lifecycle:

1. Construct `Application` from the non-owning process `HINSTANCE`.
2. Observe the same handle with `GetInstance()`.
3. Call `Run<ApplicationWindow>(show_command)`.
4. Return its message-loop result as the process exit code.
5. Return its result through the process entry point. Most applications can use
   the one-line `RunApplication<Window>()` helper instead.

`Application` is intentionally non-copyable and non-movable. It initializes the one WTL `_Module`, registers the message loop, owns the stack lifetime of `ApplicationWindow`, and cleans up in reverse order.
