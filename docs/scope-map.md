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

## Use raw Win32 through the escape hatch

These are compatible with mwtl but intentionally remain ordinary platform APIs:

- owner-drawn and custom-drawn controls beyond the provided paint hooks;
- printing and print preview;
- registry schemas beyond the focused placement helpers;
- custom window classes and uncommon control messages;
- OLE drag/drop beyond file drops;
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
- MDI framework, docking framework, Ribbon framework, or document/view model;
- 32-bit targets and non-MSVC-compatible ABIs.

## Candidate capabilities requiring an explicit project decision

Property sheets, tray abstraction, WebView2 hosting helpers, Scintilla hosting,
print helpers, and richer UI Automation support may be evaluated in future.
Until accepted into the public API and stability policy, agents must use the
composition paths above rather than invent mwtl symbols.

