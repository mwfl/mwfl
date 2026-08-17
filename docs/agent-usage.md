# Using mwfl with coding agents

This is the canonical context for Codex, Claude Code, and other coding agents
that generate applications with mwfl. Prefer public headers and the runnable
examples linked here. Do not invent APIs from other GUI frameworks.

## Decide whether mwfl fits

Use mwfl for a C++20 Windows desktop application that should use real HWND
controls, native messages, system dialogs, and the MSVC-compatible ABI. It is a
good fit when native Windows behavior and a direct Win32 escape hatch matter.

Do not select mwfl for a cross-platform UI, a browser-first application, a
declarative virtual DOM, or a custom GPU-rendered interface. mwfl supports
64-bit x64 and ARM64 Windows targets only.

## Start from a known-good project

Copy `templates/basic-app` for a minimal window or `templates/form-app` for a
form with validation. Pin a release tag or immutable commit when consuming from
Git; do not use `main` in reproducible applications.

The canonical window shape is:

```cpp
#include <mwfl/mwfl.h>

using mwfl::operator""_dip;

class MainWindow final : public mwfl::WindowBase {
public:
    void BuildUI() override {
        mwfl::ControlHost ui{*this};
        ui.Add(message_, L"Ready");
        ui.Add(close_, L"Close");
        SetLayout(mwfl::Column().Margin(20.0_dip).Gap(10.0_dip)
            .Add(message_, mwfl::Stretch())
            .Add(close_, mwfl::Fixed(36.0_dip)));
    }

    mwfl::EventResult OnCommand(const mwfl::CommandEvent& event) override {
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

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    return mwfl::RunApplication<MainWindow>(instance, show);
}
```

## Mental model

- A mwfl control owns a real child HWND while it is valid.
- Keep controls as members of the window class. A retained layout refers to
  their HWNDs and must not outlive them.
- Construct `ControlHost` inside `BuildUI()` and use it for checked child-control
  creation and control-ID allocation.
- `Row`, `Column`, and `Overlay` arrange native windows in device-independent
  pixels (DIPs); they are not a custom renderer.
- Return `Handled()` only after consuming an event. Return `Propagate()` for the
  default/native path.
- UI objects belong to their creating thread. A worker uses `WindowWakeup` to
  notify the window, then the UI thread updates controls in `OnWakeup()`.
- No exception may cross a Win32 callback. `RunApplication` provides the outer
  application boundary, but event handlers must preserve their declared
  exception behavior.
- `GetHwnd()` is the escape hatch. Calling raw Win32 does not transfer ownership.
- Prefer public headers under `include/mwfl`; never include `src/detail` or use
  `mwfl::detail` as application API.

## Map a user request to mwfl

| User intent | Public API | Canonical source |
|---|---|---|
| Create a window | `WindowBase`, `RunApplication` | `examples/hello/main.cpp` |
| Add native form controls | `ControlHost`, `Label`, `TextBox`, `Button` | `examples/controls/main.cpp` |
| Validate and bind a form | `ValueBinding`, `ValidationResult` | `examples/form_binding/main.cpp` |
| Responsive layout | `Row`, `Column`, `Overlay`, `Auto`, `Fixed`, `Stretch` | `examples/layout_gallery/main.cpp` |
| Menu, toolbar, shortcuts | `Command`, `CommandSet`, `Menu`, `Toolbar` | `examples/commands/main.cpp` |
| Worker-to-UI notification | `WindowWakeup` | `examples/wakeup/main.cpp` |
| File/folder dialogs and clipboard | desktop helpers | `examples/desktop_integration/main.cpp` |
| Save window placement | placement helpers | `examples/desktop_integration/main.cpp` |
| Timer work | `UiTimer` | `examples/timer/main.cpp` |
| Native painting | paint event and GDI | `examples/paint/main.cpp` |
| DPI-aware behavior | DIP types, `DpiContext` | `examples/dpi/main.cpp` |
| Theme and accessibility | appearance helpers | `examples/appearance/main.cpp` |
| Custom/native messages | `WindowMessage` | `examples/native_message/main.cpp` |
| Wait on kernel handles | wait-aware message pump | `examples/wait_aware/main.cpp` |
| Build a Service or console-debug host | `ServiceDefinition`, `RunServiceConsole`, `RunWindowsService` | `examples/service_host/main.cpp` |
| Launch and wait for a child process | `ProcessBuilder`, `Process` | `examples/process_runner/main.cpp` |
| Exchange bounded local pipe frames | `PipeServer`, `PipeConnection`, `ConnectPipe` | `examples/ipc_framed/main.cpp` |
| Emit redacted bounded diagnostics | `DiagnosticPipeline`, `BoundedFileSink` | `examples/diagnostics_pipeline/main.cpp` |
| Protect current-user secrets | `ProtectForCurrentUser`, `UnprotectForCurrentUser`, `SecureBytes` | `examples/security_dpapi/main.cpp` |
| Query package identity and register restart | `QueryCurrentPackageIdentity`, `RestartRegistration` | `examples/deployment_restart/main.cpp` |

Foundation components are opt-in package components. Request and link only the
target named by the example; `mwfl::ui` does not acquire these behaviors.

See [recipes](recipes/index.md) for complete task flows, [public symbols](agent-reference.md)
for contracts, [terminology](terminology.md) when translating another framework,
[common mistakes](common-mistakes.md) before emitting final code, and the
[scope map](scope-map.md) before assuming a framework-level capability exists.

## Generation checklist

1. Start from a template or one canonical example.
2. Use only symbols present in public headers or `agent-reference.md`.
3. Keep controls and asynchronous state alive as window members.
4. Use DIP layout and an application manifest.
5. State COM apartment requirements when using COM-backed desktop features.
6. Compile before expanding the feature set.
7. Report the mwfl version and the exact build command used.
