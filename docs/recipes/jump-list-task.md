# Add a Jump List task

Link `mwfl::shell`, initialize an STA, and choose a stable AppUserModelID. Give
each `JumpListTask` a stable ID; mwfl adds `--mwfl-jump-task=<id>` so removed
tasks can be honored. `CommitJumpList` aborts an incomplete transaction. Handle
`unavailable`, `rejected`, and `com_failure`, and offer `DeleteJumpList` for
cleanup.
