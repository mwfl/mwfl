## mwfl 0.1.1 (unreleased)

Version 0.1.1 is the ecosystem and compatibility release following the first
public baseline. Its executable acceptance contract is
[`docs/milestones/0.1.1.md`](../milestones/0.1.1.md).

- Freeze the complete v0.1 installed-header baseline as a metadata gate.
- Build the eight official standalone applications against each candidate MWFL
  commit in a dedicated downstream workflow.
- Preserve installed-package, `FetchContent`, and `add_subdirectory` source
  consumers across VS2022/VS2026 and x64/ARM64 maintained configurations.
- Make `verify.ps1 -Mode Full` run both Debug and Release on ARM64, matching the
  maintained presets instead of silently stopping after Debug.
- Resolve project-owned MSVC static-analysis findings in update request header
  sizing, monitor recovery nullability, and the Win32 splitter notification ID.
- Reorganize example discovery by learning path without moving canonical source
  directories or breaking existing links.
