# Build the SQLite Viewer

The SQLite Viewer is a complete, local-only database inspection application in
`examples/sqlite_viewer`. It composes mwfl controls with the WinSQLite API that
ships in the Windows SDK; SQLite is deliberately an application dependency, not
part of the mwfl core library.

## Product contract

- Databases open with `SQLITE_OPEN_READONLY`; the title and query policy make
  that mode visible.
- The schema tree includes user tables, views, indexes, and triggers. Selecting
  a table or view prepares and runs a quoted `SELECT` query.
- The SQL workspace accepts one read-only statement at a time. SQLite's own
  `sqlite3_stmt_readonly` result is authoritative, so comments and CTEs do not
  need a fragile keyword parser.
- Results are bounded to 5,000 rows. The status text reports elapsed time and
  truncation rather than silently allocating without limit.
- SQL `NULL` is visible and BLOB values are represented by byte count without
  accidentally decoding arbitrary data as text.
- CSV export writes UTF-8 with a BOM, quotes every field, doubles quotes, and
  exports only the current bounded result.
- Opening, querying, and export failures remain ordinary visible application
  errors. No exception crosses a Win32 callback.

## Native composition

The left `TreeView` owns schema navigation. A multiline `TextBox` and report
`ListView` form the query workspace. `CommandSet`, menus, accelerators, file
dialogs, file drop, responsive DIP layout, accessibility names, and DWM
appearance remain ordinary mwfl features. The application keeps the borrowed
`sqlite3*` entirely inside its RAII `Database` object.

## Validation

`mwfl.sqlite_viewer` creates a real temporary database and checks schema reads,
Unicode results, rejected writes, rejected multiple statements, row limits, CSV
output, and identifier quoting. `mwfl.sqlite_viewer_gui` opens a generated
database and exercises the populated native schema/query/results workspace.

