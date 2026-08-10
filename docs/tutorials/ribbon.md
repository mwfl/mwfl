# Build a Windows Ribbon application

Windows Ribbon is an optional interoperability component. Prefer ordinary
`CommandSet`, menus, toolbars, and the modern docking workspace unless an
Office-style Ribbon is a product requirement. Linking `mwtl::mwtl` alone never
starts COM or links the Ribbon Framework.

## 1. Prerequisites

Install Visual Studio 2026 with Desktop development with C++ and the Windows 10
or newer SDK. Open a Developer PowerShell in the repository and configure x64:

```powershell
cmake --preset vs2026-x64
cmake --build --preset vs2026-x64-debug --target mwtl_ribbon_workspace
ctest --preset vs2026-x64-debug -R "^mwtl\.ribbon_(model|native|workspace_gui)$"
```

Repeat with `vs2026-x64-release`. The build locates the SDK `uicc.exe`, compiles
`examples/ribbon_workspace/ribbon.ribbon`, and embeds the generated BML and
string resources. A missing UICC produces an explicit configure warning rather
than silently creating a nonfunctional Ribbon target.

## 2. Request the component

For an installed package:

```cmake
find_package(mwtl 0.1 CONFIG REQUIRED COMPONENTS ribbon)
target_link_libraries(my_app PRIVATE mwtl::ribbon)
target_compile_features(my_app PRIVATE cxx_std_20)
```

Add the generated `.rc` file to the executable, not the library. Resource and
application command IDs must be stable nonzero integers.

## 3. Build the pointer-free command model

Create application-owned commands first, then map Ribbon IDs to them:

```cpp
mwtl::CommandSet commands;
commands.Add(mwtl::Command({2001}, L"Save", [&] { SaveDocument(); }));

mwtl::RibbonCommandModel ribbon_model;
ribbon_model.Add({{1001}, {2001}, 1});
```

`RibbonCommandModel` owns IDs, modes, context flags, and copied recent-item
metadata. It never stores document pointers, HWNDs, or COM interfaces.

## 4. Create and load on the STA UI thread

Initialize an STA before constructing the host. The host borrows the frame,
model, and command set and owns its `IUIFramework` and callback references:

```cpp
mwtl::RibbonFrameworkHost ribbon;
auto created = ribbon.Create(frame, ribbon_model, commands);
if (created.status == mwtl::RibbonHostStatus::unavailable) {
    ShowToolbarFallback();
} else if (!created) {
    ReportRibbonError(created.error);
} else if (!ribbon.Load(instance, L"MY_RIBBON_RIBBON")) {
    ReportBrokenResource();
}
```

The suffix is emitted by UICC; inspect the generated `.rc` rather than guessing
the resource name. `GetFramework()` is borrowed and must never outlive the host.

## 5. Modes, contexts, recent items, and layout

Update the model before invalidating native properties. `SetModes` accepts a
nonzero bit mask corresponding to `ApplicationModes` in Ribbon markup.

```cpp
ribbon_model.SetActiveModes(2);
ribbon.SetModes(2);
ribbon_model.SetContextVisible({7}, true);
ribbon.InvalidateAll();
```

Use `SetRecentItems` with stable IDs; the model copies and bounds the list to
128 entries. Persist only ordinary application values such as active mode and
context state. Never persist a COM pointer or native interface.

In `WM_SIZE`, place application content below `ribbon.GetHeight()`. On
`WM_DPICHANGED` and `WM_THEMECHANGED`, update the window normally and invalidate
Ribbon properties. Keyboard access comes from UICC keytips and the application
menu; keep labels meaningful for accessibility.

## 6. Ordered shutdown and failure behavior

Stop commands and callbacks, call `Destroy` on the creating thread, destroy the
frame, then uninitialize COM. `Destroy` is idempotent. Callback exceptions are
contained; retrieve diagnostics with `TakeCallbackException`. Treat framework
unavailability as a supported fallback, but treat invalid generated resources
as a packaging defect.

The complete runnable source is `examples/ribbon_workspace/main.cpp`. Its GUI
self-test changes modes and context, adds recent items, handles theme state,
round-trips versioned settings, and runs without dialogs.
