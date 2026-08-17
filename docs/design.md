# Design and scope

mwfl (Modern Windows Foundation Layer) is a C++20 foundation for native Windows
applications. It supports three application hosts: windowed GUI applications,
Windows Services, and command-line applications. The `mwfl::ui` component
removes repeated HWND setup and lifetime code while preserving messages,
styles, return values, and direct Win32 interoperation. Foundation components
apply the same explicit ownership, structured failure, cancellation, and native
escape-hatch rules outside the UI layer.

mwfl is not a cross-platform framework, retained renderer, MVVM framework,
general networking or database library, task scheduler, or replacement for the
C++ standard library. Components are added only when they materially improve a
Windows-specific ownership, lifetime, callback, security, deployment, or
diagnostic boundary.

Raw operations preserve native success values and do not throw for ordinary
Win32 failure. Checked adapters throw `mwfl::Error`, recording the operation,
native system error, and source location. `Application` catches exceptions that
escape construction or message dispatch, reports them, destroys the HWND, and
returns failure.

Controls own their child HWND and must outlive layout entries referencing them.
Windows and controls are UI-thread-affine. `WindowWakeup` is the explicit
lifetime-safe worker-to-UI notification mechanism.

`WindowBase` is the concise virtual-handler API. `Window<T>` is the extension
path for distinct CRTP types and direct Win32 integration. Both share message
decoding and exception boundaries.

Commands describe application intent independently of menus, controls, toolbars,
and accelerators. Appearance is optional and dynamically probes DWM so the core
continues to run on the declared Windows 10 baseline. High contrast overrides
cosmetic preferences.

Binding stays deliberately local and event-agnostic: it synchronizes one control
and one model value when the application calls `Pull` or `Push`. Validation and
change suppression are included; object graphs, reflection, and global reactive
state are outside mwfl's scope.
