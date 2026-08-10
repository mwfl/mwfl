# Add a printed page

Link `mwfl::printing`. Build application-owned pages with `PaginateContent`,
then pass the selected `PrintPage` range to `PrintPages`. The render callback
receives a borrowed printer HDC and must return before it is used again. Keep
authoritative text and pagination outside `PrintJob`; return `false` from the
renderer or `true` from the optional cancellation check to make the transaction
call `AbortDoc`. Cancellation is checked before each page and callback
exceptions are contained.

Verify with `ctest --preset vs2026-x64-debug -R mwfl.printing`.
