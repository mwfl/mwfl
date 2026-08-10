# Legacy MDI recipes

- Prefer `DocumentWorkspaceModel` for new products; request `mwtl::mdi` only for
  traditional Windows MDI interoperability.
- Store documents by `MdiChildId`; never use a global current-document pointer.
- Create `MdiHost` on the frame UI thread and resize its owned MDICLIENT.
- Route active commands with `RouteMdiActiveChild`; handle reentrant activation.
- Run the application accelerator table before `MdiHost::Translate`.
- Collect all dirty close decisions before destroying any child.
- Use `Move`/`TransferTo` for pointer-free ordering and metadata transfer.
- Stop callbacks and call `Destroy` before the frame is destroyed.

See `docs/tutorials/mdi.md` and `examples/mdi_workspace`.
