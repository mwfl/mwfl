# Design and scope

mwfl (Modern Windows Foundation Layer) is a small C++20 foundation for native Windows applications. It removes
repeated setup and lifetime code while preserving HWNDs, messages, styles,
return values, and direct Win32 interoperation. It is not a cross-platform UI
toolkit, retained renderer, or MVVM framework.

Raw operations preserve native success values and do not throw for ordinary
Win32 failure. Checked adapters throw `mwfl::Error`, recording the operation,
native system error, and source location. `Application` catches exceptions that
escape construction or message dispatch, reports them, destroys the HWND, and
returns failure.

Controls own their child HWND and must outlive layout entries referencing them.
Windows and controls are UI-thread-affine. `WindowWakeup` is the explicit
lifetime-safe worker-to-UI notification mechanism.

`WindowBase` is the concise virtual-handler API. `Window<T>` is the extension
path for distinct CRTP types and direct WTL integration. Both share message
decoding and exception boundaries.

Commands describe application intent independently of menus, controls, toolbars,
and accelerators. Appearance is optional and dynamically probes DWM so the core
continues to run on the declared Windows 10 baseline. High contrast overrides
cosmetic preferences.

Binding stays deliberately local and event-agnostic: it synchronizes one control
and one model value when the application calls `Pull` or `Push`. Validation and
change suppression are included; object graphs, reflection, and global reactive
state are outside mwfl's scope.
