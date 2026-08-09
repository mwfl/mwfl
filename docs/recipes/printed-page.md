# Add a printed page

Link `mwtl::printing`. Build application-owned pages with `PaginateContent`,
then pass the selected `PrintPage` range to `PrintPages`. The render callback
receives a borrowed printer HDC and must return before it is used again. Keep
authoritative text and pagination outside `PrintJob`; return `false` or request
cancellation to make the transaction call `AbortDoc`.

Verify with `ctest --preset vs2026-x64-debug -R mwtl.printing`.
