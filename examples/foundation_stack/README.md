# Foundation stack

The 0.1.8 reference example composes three independently requested components:
the controller launches a worker with `mwfl::process`, exchanges bounded frames
through `mwfl::ipc`, and emits structured lifecycle events through
`mwfl::diagnostics`.
