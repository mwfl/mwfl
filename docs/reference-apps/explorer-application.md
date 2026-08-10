# Explorer-style application

Canonical source: `examples/explorer/main.cpp`; pure application data lives in
`examples/explorer/explorer_model.*`.

This application proves that the individual wrappers compose into a useful
native desktop shell. A Rebar hosts an image-backed Toolbar, stable Tabs select
views, a keyboard-operable Splitter owns TreeView and owner-data ListView panes,
and a responsive StatusBar reports folder, count, selection, and actions.
Context menus and accelerators route through the same `CommandSet`.

The `ExplorerModel` owns folder/file identity, filtering, cell text, images, and
sorting. No application pointer is stored in native item data. Splitter forwards
attached-pane notifications to the top-level window, where `HandleNotification`
services real `LVN_GETDISPINFO` requests and captures callback exceptions.

`mwfl.explorer_model` proves filtering and stable identities across sorts.
`mwfl.explorer_gui` launches the real executable and checks native composition,
image-list borrowing, MSAA names, TreeView navigation, virtual mapping, sorting,
keyboard Refresh, tabs, context routing, responsive pane geometry, theme/system
messages, and 40-cycle menu resource bounds.

Theme and system-setting changes reapply system appearance at runtime, so the
shared High Contrast precedence is preserved. The executable manifest test and
installed package consumer keep this composition on the distributable path.

Follow the beginner walkthrough in
[`docs/tutorials/explorer-application.md`](../tutorials/explorer-application.md).
