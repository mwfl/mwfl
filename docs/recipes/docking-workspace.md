# Docking workspace recipes

Use this page for focused changes. For the complete ownership and failure model,
read [the docking workspace tutorial](../tutorials/docking-workspace.md).

## Register a tool panel

1. Allocate a stable nonzero `DockPanelId`.
2. Add metadata with role `tool` to `DockLayoutModel`.
3. Create and retain ownership of the content HWND and application state.
4. Bind the HWND with `DockNativeWorkspaceAdapter::BindPanel`.
5. Synchronize only after the panel and its destination group are both bound.

Do not derive the ID from an HWND or vector address.

## Create document and tool regions

Construct the document root group, add tool groups with `AddDockedGroup`, and
give every group and node a stable ID. Document-role panels may enter document
groups; tool-role panels may enter tool groups. Treat a role mismatch as a
design error rather than weakening validation.

## Tab or reorder a panel

Create a `move_to_group` mutation (or `MakePinDockMutation`) and set
`target_index` when order matters. Use propose -> native prepare/adopt -> model
commit -> native commit. Activate by stable panel ID after the move.

## Close and later restore a panel

Propose `close_panel`. The native adapter parks and hides the bound HWND inside
the same rollback-capable adoption plan; your application still owns that HWND
and decides whether to destroy or retain it. Restore by resolving the same
stable ID into a validated candidate/default snapshot, never by serializing the
old HWND.

## Split a panel

Use `split_to_group` with new stable group, group-node, and split-node IDs, a
logical edge, and a bounded ratio. DIPs and ratios belong in logical state;
pixel rectangles do not. Reject ID collisions before touching HWNDs.

## Dock with the mouse

Begin `DockDragSession` after the system drag threshold. Feed screen-space DIP
targets to `UpdateTarget`, call `SetProposal`, and show `DockPreviewWindow`.
Commit only on button release. Escape, capture loss, destroyed windows, or an
invalid proposal cancel the session and retain the source snapshot and focus.

## Dock with the keyboard

Pass the same enabled targets to `DockKeyboardSession`. Use arrows, Tab, and
Shift+Tab to navigate; announce `GetSelection().announcement`; Enter accepts;
Escape cancels. Keep this path available whenever pointer docking is available.

## Float and redock

Create an owned `DockFloatingWindow`, attach a borrowed group host, and propose
`float_panel` with stable floating/group/node IDs and DIP placement. Redock with
a normal move/pin transaction, then detach and hide the empty floating host.
Closing the auxiliary window must not post process quit.

## Auto-hide and pin

Propose `auto_hide` with an edge. Drive visibility from
`DockAutoHideController`, including focus-held-open behavior and bounded delays.
Pin with `MakePinDockMutation`. Provide commands and keyboard focus paths in
addition to hover.

## Save and restore

Call `SaveDockingSessionAtomic` with the last committed snapshot. Load with
`LoadDockingSession`, resolve only known stable panel IDs, recover monitor
placement, validate/adopt the candidate, and otherwise use a safe default.
Never serialize pointers, HWNDs, callbacks, or unbounded pixel graphs.

## Add a custom target

Allocate a stable nonzero target ID, select `tab_group`, `split`, `auto_hide`,
or `floating`, provide valid screen-space DIP bounds, and assign priority where
targets overlap. Convert the accepted target to an explicit model mutation;
never let hit testing mutate the layout directly.

## Shut down safely

Cancel drag/keyboard/auto-hide activity, destroy the preview, restore and
destroy floating windows, detach the native adapter, then destroy application
content. Remove native subclasses before their state objects die. Save only
committed state.

## Focused verification

```powershell
ctest --test-dir build/presets/vs2026-x64 -C Debug --output-on-failure `
  -R "mwfl\.(docking_|manifest\.mwfl_docking_workspace_demo)"
```

Repeat with `-C Release` for the approved 0.7 local gate.
