# mwtl capability and scope map

This map prevents humans and coding agents from guessing that an API exists
because it is present in MFC, WTL, Win32++, wxWidgets, or WinUI. Public headers
remain authoritative.

## Supported directly

| Capability | Primary mwtl surface | Canonical example |
|---|---|---|
| Application and top-level window lifecycle | `Application`, `WindowBase` | `application`, `window` |
| Core HWND controls | `ControlHost`, controls | `controls` |
| Specialized Common Controls | navigation/input/command controls | `common_controls` |
| Typed keyboard, mouse, size, paint, DPI and command events | event overrides | focused examples |
| DIP row, column and overlay layout | `LayoutNode` | `layout_gallery` |
| Explicit model binding and validation | `ValueBinding` | `form_binding` |
| Shared commands, menu, toolbar and accelerators | `CommandSet` | `commands` |
| Timers, wait-aware pumping and worker wakeup | timer/pump/wakeup | `timer`, `wait_aware`, `wakeup` |
| File/folder dialogs, clipboard and file drops | desktop helpers | `desktop_integration` |
| Window placement and shell integration | desktop helpers | `desktop_integration`, `hot_corners` |
| DWM appearance and accessibility helpers | appearance helpers | `appearance` |
| Raw HWND and native message interoperability | `GetHwnd`, `WindowMessage` | `native_message` |
| Printing, preview, settings, and native jobs | `PrintPreviewModel`, `PrinterSettings`, `PrintJob` | `printing` |
| OLE data objects and drag/drop | `OleDataObjectBuilder`, OLE source/target helpers | `ole_drag_drop` |
| Versioned settings, associations, Jump Lists, taskbar, and Help | focused settings and shell helpers | `shell_integration` |
| Modern document, docking, Ribbon, MDI, EMF, and GDI+ workflows | independent optional/focused components | corresponding workspace/interoperability examples |

## Use raw Win32 through the escape hatch

These are compatible with mwtl but intentionally remain ordinary platform APIs:

- owner-drawn and custom-drawn controls beyond the provided paint hooks;
- registry schemas beyond the focused placement helpers;
- custom window classes and uncommon control messages;
- OLE formats and behaviors beyond the focused data/source/target helpers;
- shell extensions, hooks, services, and system-wide hotkeys;
- advanced accessibility providers and UI Automation peers.

Keep raw handles non-owning unless the Win32 API explicitly transfers
ownership. Convert pixels and DIPs at the boundary and preserve callback
exception safety.

## Compose a focused third-party library

Do not grow mwtl into a wrapper for these ecosystems:

| Need | Recommended composition |
|---|---|
| Web content | WebView2 SDK hosted in an HWND |
| Rich source editor | Scintilla HWND control |
| Database/network/protocol client | a non-UI C++ library plus `WindowWakeup` |
| Complex charts | a native HWND chart control or explicit custom renderer |
| Media playback | Media Foundation or a dedicated playback component |
| Embedded GPU rendering | Direct2D/Direct3D swap chain in a native host |

The application owns the third-party object's lifecycle and uses mwtl for the
surrounding native window, commands, layout, and thread handoff.

## Intentionally out of scope

- cross-platform abstraction;
- virtual DOM or declarative XAML-like runtime;
- custom widget renderer or theme engine;
- reflection, code generation, or message-map macros;
- automatic global data context;
- general-purpose task scheduler;
- 32-bit targets and non-MSVC-compatible ABIs.

## Long-term parity scope

Document applications, modern tabbed workspaces, docking, optional Ribbon,
legacy MDI, EMF/GDI+, expanded taskbar behavior, and safe contextual Help are
current public capabilities. Ribbon, MDI, graphics, and shell integrations are
independent components; none expands the minimum `mwtl::mwtl` surface. Their
dependency order and evidence are defined in the
[Win32++ capability parity roadmap](win32xx-parity-roadmap.md).

Parity is measured by the desktop application scenarios mwtl can implement. It
does not imply Win32++ or MFC API compatibility, and it does not relax the
Windows 10+, C++20, x64/ARM64, explicit-ownership design constraints.

## Candidate capabilities requiring an explicit project decision

Richer UI Automation providers, media playback, shell extensions, and
application-specific Ribbon property types require an explicit future project
decision. Agents must use the composition paths above rather than invent mwtl
symbols.
