# Build the Compare Tool

The standalone [Folder Compare](https://github.com/mwfl/folder-compare) is a native Windows interpretation
of the focused, local-only workflow demonstrated by
[GrapeCompare](https://github.com/everettjf/grapecompare). It does not share
source code with that project; it applies the same product principles to real
Windows controls and mwfl lifecycle contracts.

## Folder comparison contract

- Both directory trees are scanned recursively on a worker thread. The UI stays
  responsive and receives progress/results through `WindowWakeup`.
- Exclusions accept semicolon-separated Windows wildcard patterns. The defaults
  omit common build and source-control artifacts.
- Matching regular files use a size precheck followed by bounded chunk-by-chunk
  byte validation. Timestamps are never treated as proof that contents match.
- Directory entries, regular files, symbolic links, type conflicts, permission
  errors, left-only, right-only, different, and identical states stay explicit.
- Symbolic links are compared by target and are never traversed implicitly.
- The result list is virtual, so native row allocation does not scale with the
  number of compared paths.

## Text comparison and operations

Activating a changed regular file switches the virtual list to an aligned
side-by-side line view. Normal files use a Myers shortest-edit-script pass;
very large inputs use a memory-bounded aligned fallback. Binary files and files
above the preview limit remain comparable by bytes but are not misrepresented as
text. Final-newline differences are reported.

Copy operations require an explicit direction and confirmation. Files copy to a
temporary sibling, verify byte-for-byte, and then replace the destination with a
write-through move. Directory copies recurse without following symbolic links.
The first public version intentionally does not claim GrapeCompare's three-way
merge, Git integration, plan files, or durable undo history.

## Validation

`folder-compare.model` covers exact folder states, exclusions, final newlines,
verified copy, cancellation, and 100 deterministic randomized text-diff
reconstruction cases. Application builds, tests, and portable artifacts now
belong to the standalone repository.
