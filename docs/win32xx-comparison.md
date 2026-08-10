# Win32++ scenario comparison

This is an application-scenario map, not an API compatibility promise. The
comparison uses the Win32++ 10.2 project description, class catalogue, and
published samples as the reference inventory:

- [Win32++ project and documentation](https://win32-framework.sourceforge.net/)
- [Win32++ class description](https://win32-framework.sourceforge.net/description.htm)
- [Win32++ published sample inventory](https://sourceforge.net/projects/win32-framework/files/)

Status meanings:

- **supported** — a public mwtl API, executable example, and automated evidence
  exist in the completed/current milestone;
- **composed** — the scenario is complete by combining focused mwtl APIs rather
  than inheriting one framework base class;
- **planned** — explicitly assigned to a later 0.x milestone;
- **non-goal** — deliberately replaced by C++20/Windows 10+ facilities or raw
  Win32 interop.

## 0.2–0.3 desktop application scenarios

| Win32++ capability or sample family | mwtl answer | Status | Evidence |
|---|---|---|---|
| `CWinApp`, `CWnd`, Simple sample | `RunApplication`, `Application`, `WindowBase` with typed events and raw HWND escape | supported | `examples/hello`, lifecycle/native-message tests |
| `CFrame` menu/toolbar/status composition | Explicit `CommandSet`, `Menu`, `Toolbar`, `Rebar`, `StatusBar`, retained layout | composed | `examples/notepad`, `examples/explorer` |
| Notepad sample | Unicode SDI editor with atomic save, recent files, find/replace, single-instance activation, settings and GUI self-test | supported | `examples/notepad`, `mwtl.notepad_gui` |
| Explorer sample | Stable TreeView navigation, owner-data ListView, image-backed commands, tabs, splitter, status and context menu | supported | `examples/explorer`, `mwtl.explorer_model`, `mwtl.explorer_gui` |
| Common control wrappers (`CListView`, `CTreeView`, `CTab`, toolbar/rebar/status families) | Move-only HWND wrappers plus stable application IDs and typed notifications | supported | `examples/common_controls`, `mwtl.modern_api`, `mwtl.navigation_native` |
| Virtual/owner-data ListView | Shared application-authoritative `VirtualListModel`; no object pointers in native item data | supported | Explorer model/GUI and navigation native tests |
| Splitter and Splitter sample | DIP constraints, mouse/keyboard movement, accessible slider, direct-child pane ownership, forwarded child notifications | supported | `Splitter`, `SplitterModel`, `mwtl.splitter_native` |
| TabDemo / tabbed views | `TabWorkspaceModel` owns stable logical identity while `TabControl` projects native state | supported | common-controls/Explorer examples, tab workspace tests |
| PropertySheets sample / property-page classes | Stable pages, modal/modeless ownership, dirty/validate/apply/reset, captured callback failures | supported | Settings/property-sheet examples and native tests |
| Resizer helpers such as `CResizer` | One retained `Row`/`Column`/`Overlay` layout system shared by windows, dialogs, pages, and splitter hosts | composed | layout gallery, Settings and Explorer GUI tests |
| Menus, accelerators and image lists | Owned `Menu`/`AcceleratorTable`/`ImageList`; commands are the single presentation state source; borrowing is explicit | supported | commands/common-controls/Explorer examples and native tests |
| Task-dialog workflow | Structured buttons/radios/verification/progress/callback controller with explicit HRESULT and exception result | supported | common-controls example and `mwtl.task_dialog` |
| Notification-area utility | Stable-GUID `TrayIcon`, typed v4 events, Explorer restart recovery and deterministic removal | supported | Hot Corners and tray native tests |
| Registry-backed application settings | Versioned, focused application state with structured failures; no hidden global configuration singleton | composed | Notepad and Settings model/GUI tests |

## 0.4–0.5 rendering and native integration scenarios

| Win32++ capability or sample family | mwtl answer | Status | Evidence |
|---|---|---|---|
| ScintillaDemo | Optional pinned Scintilla HWND component with strict Unicode, typed notifications, commands, search, save points, and deterministic deployment | supported | `examples/code_editor`, Scintilla native/text/GUI tests |
| DirectX/Picture/Scribble-style rendering | Application-owned models composed with recoverable Direct2D/Direct3D hosts and bounded WIC image decode | composed | Drawing/Image Viewer examples, D2D/D3D/WIC model/native/GUI tests |
| PrintPreview and printing samples | Shared printer-independent pagination, zoom/navigation preview, printer discovery/settings, balanced job RAII, cancellation, and structured native failure | supported | `examples/printing`, printing model/job/settings/native/GUI tests |
| OLE drag/drop | Immutable bounded Unicode/file/custom/delayed data objects, typed effect negotiation, source/target helpers, and revoke-once registration | supported | `examples/ole_drag_drop`, OLE data/drag-drop/GUI tests |
| Registry settings and file associations | Typed versioned HKCU settings and owner-marked reversible per-user association plans; unrelated state is preserved | supported | `examples/shell_integration`, settings/association/stress tests |
| Jump Lists, recent destinations, and taskbar integration | Stable AppUserModel/task identity, removed-task filtering, transactional Jump Lists, recent documents, progress/overlay reset, and Explorer restart recovery | supported | `examples/shell_integration`, Shell integration/GUI/stress tests |

## 0.6 modern multi-document scenarios

| Win32++ capability or sample family | mwtl answer | Status | Evidence |
|---|---|---|---|
| Multi-document frame/workspace | Stable-ID tabbed workspaces with application-owned contents, independent dirty/undo/view state, active command projection, recently closed history, and two top-level windows | supported | `examples/document_workspace`, `mwtl.document_workspace`, `mwtl.document_workspace_gui` |
| Moving documents between frames | Destination-first metadata adoption plus borrowed page-HWND reparent, source commit, rollback, focus restoration, and no global current document | supported | `TransferDocumentWithPage`, `mwtl.document_tabs_native`, workspace stress test |
| Workspace persistence | Versioned bounded atomic sessions with `FileStamp` validation, incremental missing/changed/untrusted handling, and safe fallback | supported | `DocumentSession`, `mwtl.document_session`, document-workspace GUI self-test |
| Multi-document shutdown | Inspect/decide/commit plans, synchronous convenience and asynchronous save/commit split, exception and reentrancy containment | composed | coordination and workspace stress tests |

## Later framework scenarios

| Win32++ capability or sample family | mwtl direction | Status |
|---|---|---|
| `CDocker`, docking and RibbonDockFrame samples | Stable-ID serializable dock panels, floating windows and accessible drag previews | planned for 0.7 |
| `CRibbon` / RibbonFrame samples | Optional Windows Ribbon integration, never required by the core | planned for 0.8 |
| Traditional `CMDIFrame`/`CMDIChild` | Optional adapter after the modern multi-document workspace is complete | planned for 0.8 |

## Intentional non-goals

| Win32++ compatibility surface | mwtl policy |
|---|---|
| Win32++ class names, inheritance hierarchy, message-map syntax, source or binary compatibility | No compatibility layer; migrate by scenario to typed composition. |
| Windows versions before Windows 10 1809, x86, Visual Studio before 2022, or pre-C++20 dialects | Not supported; historical compatibility must not weaken ownership or error contracts. |
| Framework string/container/smart-pointer replacements | Use the C++20 standard library. |
| A wrapper for every Win32 call | Use focused RAII wrappers where ownership/failure is material and `GetHwnd()`/native APIs elsewhere. |
| Mandatory Document/View or legacy MDI architecture | Application-owned models and optional modern workspace composition. |

This table is updated at each reflection gate. A later milestone is not complete
merely because its row is planned; it needs the executable and automated
evidence named by that milestone's acceptance checklist.
