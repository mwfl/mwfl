# Reference applications

These tested applications demonstrate how focused mwtl features compose into
real programs. Read the guide first, then use the linked source as the canonical
implementation.

- [Settings and validated forms](settings-application.md)
- [Explorer-style application](explorer-application.md)
- [Background work and lifecycle](background-application.md)
- [Document-style commands and desktop integration](document-application.md)

## Release-quality evidence

These are reference applications, not screenshots. Each executable is built
with the repository manifest and exercised through its real Win32 entry point.

| Application | Keyboard and accessibility | DPI and High Contrast | Distribution and behavior |
|---|---|---|---|
| Settings | Native property-sheet traversal plus explicit launcher/editor MSAA names | Retained DIP page layouts; runtime theme/settings refresh reapplies system appearance, where High Contrast takes precedence | `mwtl.settings_application_gui`, model persistence tests, and its manifest test |
| Explorer | Shared accelerators, tabs, TreeView, ListView, and keyboard Splitter; MSAA names are read from all primary surfaces | DIP layout and Splitter geometry; theme/settings refresh reapplies system appearance | `mwtl.explorer_model`, `mwtl.explorer_gui`, and its manifest test |
| Hot Corners | Menu accelerators and native controls; GUI self-test reads selector/status MSAA names | DPI-specific fonts; runtime theme/settings refresh reapplies system appearance | Pure behavior and tray-state models, real Shell tests, GUI self-test, and its manifest test |
| Notepad | One command model projects menus, toolbar, and accelerators; GUI test reads editor/toolbar/status MSAA names | Responsive DIP layout, DPI font projection, and theme/settings refresh | Document/search/history/file/single-instance models, `mwtl.notepad_gui`, package consumer, and its manifest test |
| Drawing | Native buttons plus Delete/Ctrl+Z canvas input; the host has an explicit accessible name | Input and render coordinates are DIPs; theme, settings, DPI, and High Contrast flow through the reusable host | Pure document/SVG test, real D2D resource test, `mwtl.drawing_gui`, package component consumer, and manifest test |

`mwtl.appearance` directly verifies the shared appearance policy: Windows High
Contrast disables custom color/backdrop choices. The native control classes
then retain Windows roles, states, focus, and system colors.
