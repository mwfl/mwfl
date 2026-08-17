# mwfl capability and scope map

This map prevents humans and coding agents from guessing that an API exists
because it is present in MFC, WTL, Win32++, wxWidgets, or WinUI. Public headers
remain authoritative.

## Product layers

The product supports GUI, Windows Service, and CLI hosts. `mwfl::ui` is the
only native UI target. UI consumers do not acquire future non-UI components
implicitly.

The minimum foundation core is limited to dependency-light primitives shared
by all three hosts: native error values, explicit handle ownership, waiting,
cancellation, Unicode boundaries, and diagnostic event values. Service,
process/job, IPC, diagnostics sinks, security, deployment, rendering, and
third-party integrations are independently requested components.

## Supported directly

| Capability | Primary mwfl surface | Canonical example |
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
| System-following DWM, native-control, menu, and custom-drawing appearance | appearance helpers and `WindowBase` | `appearance` |
| Raw HWND and native message interoperability | `GetHwnd`, `WindowMessage` | `native_message` |
| Printing, preview, settings, and native jobs | `PrintPreviewModel`, `PrinterSettings`, `PrintJob` | `printing` |
| OLE data objects and drag/drop | `OleDataObjectBuilder`, OLE source/target helpers | `ole_drag_drop` |
| Versioned settings, associations, Jump Lists, taskbar, and Help | focused settings and shell helpers | `shell_integration` |
| Modern document, docking, Ribbon, MDI, EMF, and GDI+ workflows | independent optional/focused components | corresponding workspace/interoperability examples |

## Use raw Win32 through the escape hatch

These are compatible with mwfl but intentionally remain ordinary platform APIs:

- owner-drawn and custom-drawn controls beyond the provided paint hooks;
- third-party control-specific dark palettes beyond the propagated `AppearanceState`;
- registry schemas beyond the focused placement helpers;
- custom window classes and uncommon control messages;
- OLE formats and behaviors beyond the focused data/source/target helpers;
- shell extensions, hooks, and system-wide hotkeys;
- advanced accessibility providers and UI Automation peers.

Keep raw handles non-owning unless the Win32 API explicitly transfers
ownership. Convert pixels and DIPs at the boundary and preserve callback
exception safety.

## Compose a focused third-party library

Do not grow mwfl into a wrapper for these ecosystems:

| Need | Recommended composition |
|---|---|
| Web content | WebView2 SDK hosted in an HWND |
| Rich source editor | Scintilla HWND control |
| Database/network/protocol client | a non-UI C++ library plus `WindowWakeup` |
| Complex charts | a native HWND chart control or explicit custom renderer |
| Media playback | Media Foundation or a dedicated playback component |
| Embedded GPU rendering | Direct2D/Direct3D swap chain in a native host |

The application owns the third-party object's lifecycle and uses mwfl for the
surrounding native window, commands, layout, and thread handoff.

## Intentionally out of scope

- cross-platform abstraction;
- virtual DOM or declarative XAML-like runtime;
- custom widget renderer or application-wide skinning engine;
- reflection, code generation, or message-map macros;
- automatic global data context;
- general-purpose task scheduler;
- 32-bit targets and non-MSVC-compatible ABIs.

## Long-term coverage scope

Document applications, modern tabbed workspaces, docking, optional Ribbon,
legacy MDI, EMF/GDI+, expanded taskbar behavior, and safe contextual Help are
current public capabilities. Ribbon, MDI, graphics, and shell integrations are
independent components; none expands the minimum `mwfl::ui` surface. Their
dependency order and evidence are defined in the
[Windows desktop capability roadmap](win32xx-parity-roadmap.md).

Coverage is measured by the desktop application scenarios mwfl can implement.
It does not imply compatibility with another framework's API, and it does not
relax the Windows 10+, C++20, x64/ARM64, explicit-ownership design constraints.

## Approved foundation expansion

The approved dependency order and the explicit post-0.1.9 deferrals are
documented in [`foundation-roadmap.md`](foundation-roadmap.md). The 0.1.1-0.1.9
train ships opt-in core, Service host, process, named-pipe IPC, diagnostics,
security, and deployment slices; see
[`foundation-releases.md`](foundation-releases.md) for their exact examples.

## Candidate capabilities requiring an explicit project decision

Richer UI Automation providers, media playback, shell extensions, and
application-specific Ribbon property types require an explicit future project
decision. Agents must use the composition paths above rather than invent mwfl
symbols.
