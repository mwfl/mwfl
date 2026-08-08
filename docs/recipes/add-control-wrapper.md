# Add a public control wrapper

Use this recipe when exposing another native common control.

1. Put the public wrapper and its options in the closest public control header.
2. Keep HWND ownership move-only and UI-thread-affine; use `NativeControl` where its contract fits.
3. Add implementation source only when inline forwarding would expose unnecessary Win32 details.
4. Add an independent-header compile probe, creation/lifetime test, and notification test.
5. Add the control to the appropriate compiled gallery and to `docs/api-index.json`.
6. Run `./scripts/verify-change.ps1 -ChangedFiles include/mwtl/<header> -Execute`.

Do not expose `mwtl::detail`, silently allocate unstable command IDs, or hide the native HWND escape hatch.
