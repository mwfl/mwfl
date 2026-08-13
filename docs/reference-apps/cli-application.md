# CLI application

A CLI executable is a first-class mwfl host even though it creates no HWND.
Use a normal console target rather than `WIN32`, keep `wmain` as the thin host
boundary, parse Unicode arguments into values, and return stable process exit
codes. The compiled `examples/cli_basic` program demonstrates command routing;
`examples/cli_worker` demonstrates cooperative background shutdown with
`std::jthread` and `std::stop_token`.

These examples deliberately use the C++20 standard library directly. A CLI
does not link `mwfl::ui` merely to claim framework usage. Future
`mwfl::core`, `mwfl::process`, `mwfl::ipc`, and `mwfl::diagnostics` components
will be usable without acquiring WTL, HWND initialization, or a GUI subsystem.

Use stable exit codes: zero for success, a documented nonzero value for usage
errors, and distinct values when callers need to distinguish cancellation from
native failure. Write normal output to stdout and diagnostics to stderr.
