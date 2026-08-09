# Win32++ capability parity roadmap

## Objective

mwtl aims to cover the native Windows desktop application scenarios of
established frameworks such as Win32++, while providing a focused Windows 10+,
C++20, CMake-native, DPI-correct, and coding-agent-friendly development model.

Parity means **application-scenario and capability parity**, not API, class
hierarchy, source, or binary compatibility. mwtl will not inherit compatibility
costs for obsolete Windows versions, 32-bit targets, old compilers, message-map
macros, runtime reflection, or an MFC-style mandatory Document/View framework.

## Non-negotiable constraints

- Windows 10 version 1809 or newer.
- x64 and ARM64; no x86 target.
- C++20.
- Visual Studio 2022 and 2026 with MSVC; clang-cl on x64.
- Modern CMake targets, `FetchContent`, and `find_package` consumption.
- RAII, typed events, explicit ownership, explicit thread affinity, and explicit
  failure semantics.
- DIP-based Per-Monitor-V2-aware layout.
- Raw HWND and native-message escape hatches remain available.
- Large third-party ecosystems such as WebView2 and Scintilla are composed as
  optional integrations, not reimplemented in the core library.
- Historical compatibility must not weaken the public API.

## Capability map

| Area | Current position | Target |
|---|---|---|
| Application, windows, dialogs, native controls | Core available | Complete composition and dialog workflows |
| Menus, toolbars, accelerators | Core available | State, image, customization, and status integration |
| Files, clipboard, file drops | Core available | Rich document and OLE drag/drop workflows |
| Responsive layout and DPI | Core available | Apply consistently to every framework component |
| Document applications | Examples and recipes | Supported SDI and multi-document application model |
| Status bar and rebar | Partial or absent | Direct support |
| Tabs, splitters, property sheets, tray | Absent | Direct support |
| Printing and print preview | Raw Win32 escape hatch | Focused supported API |
| WebView2, Scintilla, Direct2D/3D | Manual composition | Official optional integrations |
| Tabbed multi-document workspace | Absent | Direct support before legacy MDI |
| Docking workspace | Absent | Modern, serializable, DPI-safe docking model |
| Windows Ribbon and legacy MDI | Absent | Optional late-stage integration modules |

The detailed milestone inventory lives in machine-readable `docs/parity.json`.
[`win32xx-comparison.md`](win32xx-comparison.md) maps representative Win32++
capabilities and samples to supported, composed, planned, or intentional
non-goal outcomes with executable evidence.

## Version roadmap

Machine-readable milestone state is tracked in [`parity.json`](parity.json).
Detailed acceptance contracts live under [`milestones/`](milestones/), and a
version cannot advance until its evidence-backed completion reflection exists
under [`reflections/`](reflections/).

### 0.2 — Real SDI application foundation

Prove the library with a complete, useful Notepad-class application:

- document lifecycle: new, open, save, save as, dirty state, and close prompts;
- recent files, settings persistence, and single-instance/open-file handling;
- status bar and richer command state (`enabled`, `checked`, `visible`, text,
  and icon);
- cut/copy/paste, undo/redo, find/replace, shortcuts, and file drops;
- a complete `examples/notepad` application, tutorial, tests, Agent recipe, and
  distributable executable.

The API must be extracted from friction observed while building the application,
not designed speculatively before the application exists.

**Exit criterion:** a beginner can build a useful text editor with menus,
toolbar, status, shortcuts, file operations, and safe unsaved-change handling.

### 0.3 — Desktop controls and window composition

- status bar, rebar, splitter, tab workspace;
- property sheets/pages and task dialogs;
- coherent modal and modeless dialog lifetime;
- tray icon, richer tooltips, image lists, context menus;
- resizable dialog layout;
- richer ListView and TreeView selection, sorting, editing, notification, and
  virtual-data models;
- data adapters for common selection controls.

Reference applications should include a settings application, tray utility,
property sheet, and Explorer-style split view.

### 0.4 — Rendering and third-party HWND integrations

- reusable native-host lifecycle and focus/navigation contract;
- optional WebView2 and Scintilla CMake components;

The 0.4 implementation uses isolated `mwtl::webview2` and
`mwtl::scintilla` targets rather than placing third-party dependencies in the
core target. WebView2 has structured Runtime discovery, asynchronous
environment/controller ownership, navigation, focus/accelerator routing,
process recovery, and deterministic close. Scintilla has pinned binary/source
identity, strict Unicode conversion, typed notifications, UTF-8 byte positions,
search/replace, save points, and deployment support. `examples/browser` and
`examples/code_editor` are complete offline-testable reference applications.
- Direct2D host and Direct3D swap-chain host;
- COM event subscription RAII and asynchronous initialization/shutdown safety;
- DPI-correct third-party child-window hosting.

Reference applications should cover a browser, code editor, drawing surface,
and image viewer.

### 0.5 — Printing, drag/drop, and shell integration

- printer enumeration, print jobs, page setup, print dialog, pagination, and
  print preview;
- OLE data objects, drag sources, drop targets, and custom clipboard formats;
- file associations, Jump Lists, taskbar integration, and optional modern
  notifications;
- transparent, focused settings/registry helpers rather than a hidden global
  configuration system.

### 0.6 — Modern multi-document workspace

- document manager and tabbed document workspace;
- active-document command routing and per-document undo state;
- session restore, recently closed documents, document movement between
  windows, and coordinated shutdown;
- optional separation of document and view without a mandatory MFC-style
  Document/View architecture.

This modern tabbed model is higher priority than legacy Windows MDI.

### 0.7 — Docking workspace

- dock panels, split containers, tabbed groups, floating windows, and auto-hide;
- drag previews and accessible docking indicators;
- logical, stable-ID-based layout serialization and restoration;
- document/tool region separation;
- correct multi-monitor, DPI, keyboard, high-contrast, shutdown, and redocking
  behavior.

Reference applications should include a small IDE-style workspace and saved
multi-monitor layouts. Docking begins only after the document and tab workspace
models are stable.

### 0.8 — Optional traditional Windows frameworks

- Windows Ribbon Framework integration bound to `CommandSet`;
- a legacy MDI host for applications that genuinely require it;
- enhanced metafile and richer GDI/GDI+ helpers;
- additional taskbar and Windows help integration.

Ribbon and MDI belong in optional components and must not enlarge the minimum
core application model.

### 0.9 — Migration, ecosystem, and hardening

- migration guides from Win32++, WTL, and raw Win32;
- concept mapping without compatibility aliases;
- complete Notepad, Explorer, and mini-IDE reference applications;
- compile-time, dispatch-performance, lifetime, shutdown, DPI, multi-monitor,
  suspend/resume, and accessibility validation;
- source compatibility fixtures and deprecation tooling;
- package-manager distribution evaluation.

### 1.0 — Stability commitment

1.0 requires scenario evidence rather than a target class count:

- representative Win32++ application categories have executable mwtl answers;
- core APIs have been validated by at least three substantial applications;
- SDI, tabbed documents, and docking have complete beginner-to-advanced paths;
- ownership, threading, errors, exceptions, and compatibility policies are
  documented and enforced;
- public APIs have compatibility fixtures;
- common scenarios do not require internal types;
- documentation, examples, packages, tests, and Agent metadata agree.

## Agent-first definition of done

Every public capability must ship as a vertical slice. As applicable, a change
includes:

```text
include/mwtl/<feature>.h
src/<feature>.cpp
tests/<feature>_tests.cpp
examples/<feature>/
docs/recipes/<feature>.md
docs/api.md
docs/capabilities.json
docs/change-matrix.json
agent-evals/<feature>/
site/components/<feature>.html
```

Machine-readable context must state intent, symbols, headers, minimum example,
ownership, lifetime, thread constraints, failure semantics, composition points,
known-invalid usage, and the verification command.

### Validation phases

For 0.4 through 0.8, feature completion is intentionally judged on the local
primary matrix only: Visual Studio 2026, MSVC C++20, x64, in both Debug and
Release. That gate includes applicable unit, lifetime, DPI, keyboard,
documentation, package-consumer, and raw HWND interoperability checks.

ARM64, Visual Studio 2022, clang-cl, static analysis, sanitizers, coverage,
Pipeline, and GitHub Actions are deferred work during these releases. Existing
support files may remain, but failures in that deferred matrix neither block
feature development nor justify unrelated compatibility changes before 0.8 is
complete.

After 0.8, run a dedicated compatibility and CI stabilization phase. A feature
is ready for the subsequent broadly supported release only after the maintained
x64/ARM64 toolchains, relevant Debug/Release configurations, clang-cl where
supported, static analysis, sanitizer-compatible paths, package consumers, and
remote workflows pass again.

## Immediate execution order

1. Build and maintain the detailed human- and machine-readable parity matrix.
2. Define the 0.2 Notepad acceptance checklist.
3. Implement the Notepad with current public APIs first.
4. Record real friction; promote only reusable solutions into the library.
5. Complete the application, tests, tutorial, Agent recipe, and packaging.
6. Use that evidence to finalize the 0.3 control and workspace APIs.

Docking, Ribbon, and legacy MDI must not jump ahead of the document, command,
tab-workspace, and real-application foundations on which they depend.
