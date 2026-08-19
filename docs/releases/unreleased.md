## mwfl unreleased (next after 0.2.0)

### Changed

- Public application catalogs now list only the eight public organization
  applications and no longer expose private-project links.
- Historical milestone reflections are explicitly identified as point-in-time
  evidence; the current compatibility and CI matrix is authoritative in the
  release-readiness documentation and live workflows.
- Release workflows now produce and attest x64 and ARM64 library and Notepad
  packages. The supported compiler remains MSVC so the compatibility promise
  stays aligned with the Windows-only product boundary.
