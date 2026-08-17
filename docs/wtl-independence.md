# WTL independence

mwfl no longer depends on Windows Template Library (WTL) or ATL. The removal
is complete across public headers, implementation, CMake packages, tests,
templates, examples, developer tooling, and the documentation site.

## Native replacements

| Former responsibility | mwfl implementation |
|---|---|
| Application module and per-thread loop lookup | `mwfl::Application` and `mwfl::MessageLoop::Current()` |
| Message pump and pre-translate filters | `mwfl::MessageLoop` and `mwfl::MessageFilter` |
| Window-class registration and HWND/object binding | non-template `detail::WindowCore` trampoline using `RegisterClassExW`, `CreateWindowExW`, `WM_NCCREATE`, and `GWLP_USERDATA` |
| Frame default message dispatch | typed mwfl handlers followed by `DefWindowProcW` |
| Accelerator translation | `detail::AcceleratorFilter` registered with the active message loop |

The native engine preserves creation-failure handling, exception containment,
default geometry, configurable quit-on-destroy behavior, accelerator ordering,
and deterministic cleanup at `WM_NCDESTROY`. Raw Win32 remains the interop
escape hatch through `GetHwnd()` and the virtual `ProcessWindowMessage` hook.

## Validation gate

The repository must configure without a WTL source directory or CMake target.
The full build includes every first-party example, public-header isolation,
installed-package consumption, lifecycle failure injection, native GUI tests,
and repeated `Application` runs in one process.
`mwfl.window_core` additionally creates distinct `Window<T>` types with the
same default class name in one process and exercises rejection, custom dispatch,
repeated creation, and `WM_NCDESTROY` detachment.
