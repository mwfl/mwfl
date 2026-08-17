<p align="center">
  <img src="docs/images/mwfl-mark.svg" width="128" alt="mwfl logo">
</p>

<h1 align="center">mwfl</h1>

<p align="center"><strong>Modern Windows Foundation Layer</strong></p>

<p align="center">Native Windows UI, without the Win32 ceremony.</p>

<p align="center">
  Build real HWND applications with typed C++20 events, responsive DPI-aware
  layout, explicit ownership, and an escape hatch to Win32 at every layer.
</p>

<p align="center">
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-MIT-17a589.svg" alt="MIT license"></a>
  <img src="https://img.shields.io/badge/C%2B%2B-20-146c94.svg" alt="C++20">
  <img src="https://img.shields.io/badge/platform-Windows%2010%2B%20x64%20%7C%20ARM64-ff9f43.svg" alt="Windows 10 or newer on x64 and ARM64">
</p>

<p align="center">
  <a href="#quick-start">Quick start</a> &middot;
  <a href="#installation">Installation</a> &middot;
  <a href="#examples">Examples</a> &middot;
  <a href="https://mwfl.github.io/components/">Components</a> &middot;
  <a href="https://mwfl.github.io/">Documentation</a>
</p>

mwfl, the Modern Windows Foundation Layer, is growing from its mature native UI
layer into a foundation shared by GUI, Windows Service, and CLI applications.
The canonical `mwfl::ui` component wraps real HWND controls with clear
ownership, typed events, checked setup helpers, and DPI-aware responsive
layout. It reduces Win32 ceremony without hiding native handles, messages,
styles, or return values. Non-UI foundation components remain independently
requested so UI applications acquire no hidden service or background runtime.

<p align="center">
  <a href="https://mwfl.github.io/"><img width="100%" src="docs/images/showcase/mwfl-showcase-40s.gif" alt="MWFL applications running with populated native controls"></a>
</p>

**v0.1.9 is the current public preview.** The repository contains **54 compiled
examples** plus six standalone applications in the mwfl organization. The downloadable release packages are VS2026/MSVC x64; source and
CI cover x64 and ARM64. WebView2, Scintilla, graphics, imaging, printing, OLE,
Shell, Ribbon, and MDI integrations remain separately requested optional
components.

## Why mwfl

| Native by design | Modern where it matters | Small and transparent | Ready for real desktop work |
|---|---|---|---|
| Real HWND controls, Win32 messages, styles, handles, and return values remain available. | Typed events, RAII resources, C++20 ranges, checked setup, and responsive layout remove repetitive plumbing. | No custom renderer, virtual DOM, code generator, reflection system, or message-map macros. | Per-Monitor V2 DPI, accessibility helpers, shell integration, worker wakeups, and wait-aware pumping. |

Use mwfl when you want Windows to look and behave like Windows, while keeping
application code readable enough to reason about.

## Quick start

```cpp
#include <mwfl/mwfl.h>

using mwfl::operator""_dip;

class MainWindow final : public mwfl::WindowBase {
public:
    void BuildUI() override {
        SetTitle(L"Hello, mwfl");

        mwfl::ControlHost ui{*this};
        ui.Add(message_, L"A native Windows UI with modern C++20 ergonomics.");
        ui.Add(close_, L"Close");

        SetLayout(
            mwfl::Column()
                .Margin(24_dip)
                .Gap(12_dip)
                .Add(message_, mwfl::Auto())
                .Add(close_, mwfl::Fixed(36_dip)));
    }

    mwfl::EventResult OnCommand(
        const mwfl::CommandEvent& event) override {
        if (event.IsClicked(close_)) {
            Close();
            return mwfl::EventResult::Handled();
        }
        return mwfl::EventResult::Propagate();
    }

private:
    mwfl::Label message_;
    mwfl::Button close_;
};

int WINAPI wWinMain(
    HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
    return mwfl::RunApplication<MainWindow>(instance, show_command);
}
```

That is a complete native application. `ControlHost` allocates control IDs,
`SetLayout` rearranges the real child windows on resize, and the window
overrides only the events it needs. The underlying HWNDs remain available for
anything outside the wrapper surface.

## Installation

The recommended integration is CMake `FetchContent`:

```cmake
cmake_minimum_required(VERSION 3.21)
project(my_app LANGUAGES CXX)

include(FetchContent)
FetchContent_Declare(mwfl
  GIT_REPOSITORY https://github.com/mwfl/mwfl.git
  GIT_TAG v0.1.9
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(mwfl)

add_executable(my_app WIN32 main.cpp)
target_link_libraries(my_app PRIVATE mwfl::ui)
```

The example pins the latest public-preview release. Use an immutable commit only
when deliberately testing unreleased changes.

```powershell
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Debug
```

Use `Visual Studio 17 2022` with Visual Studio 2022. The complete
[setup guide](https://mwfl.github.io/building.html) also covers an
installed `find_package` package, the Visual Studio folder workflow, VS Code,
offline WIL sources, and building this repository.

## What is included

- application and message-loop lifetime management;
- `WindowBase` with typed keyboard, mouse, resize, timer, DPI, command,
  notification, paint, and native-message events;
- 27 native child-control wrappers;
- automatic control IDs and source-located checked operations;
- C++20 range-based batch population;
- DPI-aware row, column, and overlay layout;
- menus, accelerators, modern file/folder dialogs, clipboard, shell drops,
  window placement, custom modal/modeless dialogs, task dialogs, image lists,
  tooltips, and RAII notification-area icons;
- wait-aware message pumping and lifetime-safe worker wakeups.
- complete SDI and modern multi-document workflows, session restore, command
  routing, docking, floating windows, auto-hide, and optional legacy MDI;
- printing and preview, OLE drag/drop, versioned settings, file associations,
  Jump Lists, taskbar integration, contextual Help, Ribbon, EMF, and GDI+;
- optional, isolated Direct2D, Direct3D, WIC imaging, pinned Scintilla, and
  pinned WebView2 components with reference applications and offline self-tests.

The native UI layer is linked explicitly as `mwfl::ui`. Optional components are
requested explicitly. For example, configure with
`MWFL_BUILD_WEBVIEW2=ON` and link `mwfl::webview2`, or configure with
`MWFL_BUILD_SCINTILLA=ON`, link `mwfl::scintilla`, and call
`mwfl_deploy_scintilla(your_target)`. Core-only consumers do not fetch, link,
or deploy either dependency.

The [component reference](https://mwfl.github.io/components/) shows
every control with current code, a native screenshot, and its runnable example.
The [complete capability catalog](https://mwfl.github.io/components/catalog.html)
maps every public workflow to exact symbols, headers, examples, guides,
contracts, and tests.
Detailed API notes live in [docs/api.md](docs/api.md).

## Choose your layer

| Layer | Start here when you need | Entry point |
|---|---|---|
| UI | windows, controls, events, layout, DPI, timers, worker handoff | `mwfl::ui`; [UI guide](docs/README.md#core) |
| Application | commands, forms, documents, dialogs, settings, persistence | [Application guide](docs/README.md#application) |
| Advanced | docking, printing, OLE, Shell, Ribbon, MDI, graphics | [Advanced guide](docs/README.md#advanced) |
| Optional | Direct2D/3D, WIC, WebView2, Scintilla | [Optional guide](docs/README.md#optional) |

## Examples

The repository contains 54 independently buildable examples. Each link opens
the complete source. Start with **Hello** for the smallest application,
**Controls** for the basic widget set, or **Hot corners** for a complete
multi-monitor utility.

<details>
<summary><strong>Browse all 54 runnable examples and six standalone applications</strong></summary>

<br>

| Example | Source | Focus |
|---|---|---|
| Hello | [examples/hello/main.cpp](examples/hello/main.cpp) | Smallest complete native application |
| Application | [examples/application/main.cpp](examples/application/main.cpp) | Startup, message loop, and exit lifecycle |
| Window | [examples/window/main.cpp](examples/window/main.cpp) | HWND access, typed events, and native messages |
| Native message | [examples/native_message/main.cpp](examples/native_message/main.cpp) | Application-defined `WM_APP` messages |
| Keyboard | [examples/keyboard/main.cpp](examples/keyboard/main.cpp) | Typed key input |
| Mouse | [examples/mouse/main.cpp](examples/mouse/main.cpp) | Client coordinates and clicks |
| Resize | [examples/resize/main.cpp](examples/resize/main.cpp) | Client size and window state |
| Timer | [examples/timer/main.cpp](examples/timer/main.cpp) | RAII timers with `std::chrono` |
| Paint | [examples/paint/main.cpp](examples/paint/main.cpp) | Native GDI drawing |
| Minimum size | [examples/minmax/main.cpp](examples/minmax/main.cpp) | `WM_GETMINMAXINFO` |
| Close policy | [examples/close_policy/main.cpp](examples/close_policy/main.cpp) | Typed close interception |
| Window state | [examples/window_state/main.cpp](examples/window_state/main.cpp) | Restore, minimize, and maximize |
| DPI | [examples/dpi/main.cpp](examples/dpi/main.cpp) | Per-monitor DPI behavior |
| Window options | [examples/window_options/main.cpp](examples/window_options/main.cpp) | Styles, resources, and DIP bounds |
| Wait-aware pump | [examples/wait_aware/main.cpp](examples/wait_aware/main.cpp) | Handles and efficient idle work |
| Wakeup | [examples/wakeup/main.cpp](examples/wakeup/main.cpp) | Safe worker-to-window notification |
| COM STA | [examples/com_sta/main.cpp](examples/com_sta/main.cpp) | COM apartment lifecycle |
| CLI basic | [examples/cli_basic/main.cpp](examples/cli_basic/main.cpp) | Unicode command routing and stable exit codes |
| CLI worker | [examples/cli_worker/main.cpp](examples/cli_worker/main.cpp) | Cooperative `std::jthread`/`stop_token` shutdown |
| Foundation Core | [examples/core_foundation/main.cpp](examples/core_foundation/main.cpp) | Errors, handles, Unicode, cancellable wait |
| Service Host | [examples/service_host/main.cpp](examples/service_host/main.cpp) | Service state, console host, and SCM dispatcher |
| Process Runner | [examples/process_runner/main.cpp](examples/process_runner/main.cpp) | Structured launch, quoting, wait, and exit |
| Framed IPC | [examples/ipc_framed/main.cpp](examples/ipc_framed/main.cpp) | Bounded local Named Pipe frames |
| Diagnostics Pipeline | [examples/diagnostics_pipeline/main.cpp](examples/diagnostics_pipeline/main.cpp) | Structured redaction and bounded sinks |
| DPAPI Security | [examples/security_dpapi/main.cpp](examples/security_dpapi/main.cpp) | Current-user protection and secure clearing |
| Deployment Lifecycle | [examples/deployment_restart/main.cpp](examples/deployment_restart/main.cpp) | Package identity and scoped restart registration |
| Foundation Stack | [examples/foundation_stack/main.cpp](examples/foundation_stack/main.cpp) | Process, IPC, and diagnostics composition |
| Foundation Overview | [examples/foundation_overview/main.cpp](examples/foundation_overview/main.cpp) | All Foundation targets acquired together |
| Controls | [examples/controls/main.cpp](examples/controls/main.cpp) | Complete form-control gallery |
| Common Controls | [examples/common_controls/main.cpp](examples/common_controls/main.cpp) | Complete specialized-control gallery |
| Self-drawn host | [examples/self_drawn_host/main.cpp](examples/self_drawn_host/main.cpp) | Worker-driven native drawing |
| System lifecycle | [examples/system_lifecycle/main.cpp](examples/system_lifecycle/main.cpp) | Power, display, IME, and session messages |
| Hot corners | [examples/hot_corners/main.cpp](examples/hot_corners/main.cpp) | Complete multi-monitor utility with topology-aware outer corners |
| Form binding | [examples/form_binding/main.cpp](examples/form_binding/main.cpp) | Live model binding, validation, and explicit push/pull flow |
| Commands | [examples/commands/main.cpp](examples/commands/main.cpp) | One command model shared by menu, toolbar, and accelerators |
| Desktop integration | [examples/desktop_integration/main.cpp](examples/desktop_integration/main.cpp) | Modern dialogs, clipboard, drag-drop, and window placement |
| Document state | [examples/document_state/main.cpp](examples/document_state/main.cpp) | Dirty-state transitions and close decisions |
| Notepad | [examples/notepad/main.cpp](examples/notepad/main.cpp) | Complete Unicode SDI editor with safe file operations |
| Document workspace | [examples/document_workspace/main.cpp](examples/document_workspace/main.cpp) | Multi-window documents, active command routing, transfer rollback, and session restore |
| Appearance | [examples/appearance/main.cpp](examples/appearance/main.cpp) | Color modes, DWM backdrops, corners, and accessibility |
| Layout gallery | [examples/layout_gallery/main.cpp](examples/layout_gallery/main.cpp) | Responsive nested row, column, overlay, and sizing recipes |
| Settings | [examples/property_sheet/main.cpp](examples/property_sheet/main.cpp) | Persistent property pages with validation and Apply/OK/Cancel |
| Explorer | [examples/explorer/main.cpp](examples/explorer/main.cpp) | Rebar, commands, stable navigation, virtual data, tabs, and splitter |
| Folder Explorer | [mwfl/folder-explorer](https://github.com/mwfl/folder-explorer) | Standalone bounded folder inventory, PE inspection, signature metadata, and optional AI analysis |
| SQLite Viewer | [mwfl/sqlite-viewer](https://github.com/mwfl/sqlite-viewer) | Standalone read-only schema browser, SQL workspace, bounded results, and CSV export using Windows SQLite |
| Hex Editor | [mwfl/hex-editor](https://github.com/mwfl/hex-editor) | Standalone read-only-first binary inspector with overwrite editing, theme-aware coloring, search, and backed-up atomic saves |
| Folder Compare | [mwfl/folder-compare](https://github.com/mwfl/folder-compare) | Standalone local file/folder comparison, exact validation, text diff, cancellation, and verified copy |
| Drawing | [examples/drawing/main.cpp](examples/drawing/main.cpp) | Optional Direct2D host, DPI-aware input, device recovery, and SVG export |
| Image Viewer | [examples/image_viewer/main.cpp](examples/image_viewer/main.cpp) | WIC decode, color metadata, Fit/zoom/pan, and recoverable D2D pixels |
| Code Editor | [examples/code_editor/main.cpp](examples/code_editor/main.cpp) | Optional pinned Scintilla, Unicode files, search/replace, and notifications |
| Browser | [examples/browser/main.cpp](examples/browser/main.cpp) | Optional pinned WebView2, offline startup, process recovery, and navigation |
| PDF Reader | [mwfl/pdf-reader](https://github.com/mwfl/pdf-reader) | Standalone local PDF reader with native tabs, drag-and-drop, shortcuts, placement, and WebView2 recovery |
| Markdown Editor | [mwfl/markdown-editor](https://github.com/mwfl/markdown-editor) | Standalone local Markdown editing with Scintilla, safe offline preview, atomic saves, and GUI self-test |
| Docking Workspace | [examples/docking_workspace/main.cpp](examples/docking_workspace/main.cpp) | IDE-style documents and tools with docking, floating, auto-hide, keyboard operation, and layout restore |
| Printing | [examples/printing/main.cpp](examples/printing/main.cpp) | Shared pagination, preview, printer settings, and balanced native print jobs |
| OLE drag/drop | [examples/ole_drag_drop/main.cpp](examples/ole_drag_drop/main.cpp) | Unicode, files, and custom formats through native OLE drag/drop |
| Shell integration | [examples/shell_integration/main.cpp](examples/shell_integration/main.cpp) | Associations, Jump Lists, taskbar state, and versioned settings |
| Ribbon Workspace | [examples/ribbon_workspace/main.cpp](examples/ribbon_workspace/main.cpp) | Optional Windows Ribbon commands, modes, context, recent items, and fallback |
| MDI Workspace | [examples/mdi_workspace/main.cpp](examples/mdi_workspace/main.cpp) | Optional traditional MDI with stable child identity and coordinated close |
| Graphics Interop | [examples/graphics_interop/main.cpp](examples/graphics_interop/main.cpp) | Enhanced metafile lifecycle and bounded GDI+ PNG export |

</details>

<p align="center">
  <a href="examples/form_binding/main.cpp"><img width="48%" src="docs/images/examples/form-binding.png" alt="Form binding example"></a>
  <a href="examples/layout_gallery/main.cpp"><img width="48%" src="docs/images/examples/layout-gallery.png" alt="Responsive layout gallery example"></a>
</p>

<p align="center">
  <a href="https://github.com/mwfl/markdown-editor"><img width="96%" src="docs/images/examples/markdown-editor.png" alt="mwfl Markdown Editor with native source editing and offline preview"></a>
</p>

The [examples catalog](examples/README.md) lists every target and its run
commands. The [website](https://mwfl.github.io/) highlights selected
content-rich examples with screenshots and source links.
Screenshot and product-quality reviews follow the
[example visual acceptance checklist](docs/visual-acceptance.md).

## Build this repository

The authoritative 0.1 public-preview development and acceptance environment is Windows 10
1809 or newer, x64, Visual Studio 2026 with MSVC and a Windows SDK,
CMake 3.21 or newer, and C++20.

```powershell
cmake --preset vs2026-x64
cmake --build --preset vs2026-x64-debug
ctest --preset vs2026-x64-debug
```

Build and test Release:

```powershell
cmake --build --preset vs2026-x64-release
ctest --preset vs2026-x64-release
```

For repository development, `./scripts/doctor.ps1` discovers the supported
toolchains and `./scripts/verify.ps1 -Mode Fast` runs the standard edit loop.
If the preset build tree is unavailable or read-only, pass an independent
writable directory with `-BuildRoot build-local`.

The project rejects non-Windows, non-MSVC, and 32-bit
configurations. The v0.1.0 validation matrix contains 170 core tests and
173 tests with all optional integrations enabled. Both VS2026 x64 Debug and
Release optional matrices pass, including public-header independence, package
consumption,
manifests, examples, GUI self-tests, resource lifetime, API surface, and
deterministic property cases. VS2022, ARM64, sanitizers, coverage,
static analysis, and GitHub Actions provide additional compatibility and
quality coverage.

## Dependencies

CMake fetches a pinned Microsoft WIL revision. Exact sources
and licenses are recorded in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
Both include directories propagate through `mwfl::ui`, so applications may
`#include <wil/resource.h>` for WIL's RAII helpers; no public mwfl header
includes WIL for you. For controlled environments, see the
[offline setup instructions](https://mwfl.github.io/building.html#offline).

## Documentation

- [Windows desktop capability roadmap](docs/win32xx-parity-roadmap.md)
- [Windows application foundation roadmap](docs/foundation-roadmap.md)
- [First public preview readiness plan](docs/release-readiness.md)
- [Modern Windows UI API coverage plan](docs/windows-ui-modernization-plan.md)
- [IDE-style docking workspace tutorial](docs/tutorials/docking-workspace.md)

- [Using mwfl with coding agents](docs/agent-usage.md)
- [Copyable application templates](templates/)
- [Task recipes](docs/recipes/)
- [Agent-oriented public API contracts](docs/agent-reference.md)
- [Machine-readable task-to-API index](docs/api-index.json)
- [Capability and scope map](docs/scope-map.md)
- [Coding-agent evaluation suite](agent-evals/)
- [Compact agent context](docs/llms.txt)
- [Get started](https://mwfl.github.io/building.html)
- [Component reference](https://mwfl.github.io/components/)
- [Current API notes](docs/api.md)
- [Public header reference](docs/reference.md)
- [Design and scope](docs/design.md)
- [API stability](docs/stability.md)
- [Accessibility and keyboard checklist](docs/accessibility.md)
- [System-message recipes](docs/system-message-recipes.md)
- [Contributing](CONTRIBUTING.md)

## License

[MIT](LICENSE). WIL and other third-party dependencies retain their own
licenses.

## Acknowledgements

mwfl builds on decades of Windows desktop engineering. Thank you to the teams
and communities behind the Windows API, MFC, WIL, and Win32++ for the
designs, documentation, examples, and hard-won lessons that helped make native
Windows development more approachable. Thanks also to the WebView2 and
Scintilla projects for the optional native integrations used by the reference
applications.
