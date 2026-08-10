# Printing, OLE, and Shell applications

This tutorial uses Visual Studio 2026, MSVC C++20, and x64 on Windows 10 1809
or later. The three components are independent: link only the capability your
application needs. None is pulled into `mwfl::mwfl` automatically.

## 1. Configure and build the reference applications

Open Developer PowerShell for Visual Studio 2026 at the repository root:

```powershell
cmake --preset vs2026-x64
cmake --build --preset vs2026-x64-debug --target `
  mwfl_printing_demo mwfl_ole_drag_drop_demo mwfl_shell_integration_demo
```

Run each application:

```powershell
& .\build\presets\vs2026-x64\examples\printing\Debug\mwfl_printing_demo.exe
& .\build\presets\vs2026-x64\examples\ole_drag_drop\Debug\mwfl_ole_drag_drop_demo.exe
& .\build\presets\vs2026-x64\examples\shell_integration\Debug\mwfl_shell_integration_demo.exe
```

Printing preview works without a printer. The Print button reports a missing or
denied printer instead of changing the preview model. OLE requires
`ComApartment::ole_sta`. Shell Jump Lists require an STA and may report
`unavailable` or `rejected` when Explorer is absent or policy disables them.

## 2. Run deterministic tests

These tests require no public network, printer interaction, or user input:

```powershell
ctest --test-dir build\presets\vs2026-x64 -C Debug `
  -R "mwfl\.(printing|ole_|file_association|settings_store|shell_integration)" `
  --output-on-failure
```

The GUI self-tests are launched with `--self-test`. The Shell test writes only
under a unique `HKCU\Software\mwfl\Tests\ShellDemo-<pid>` identity, disables
Shell notification, removes owned values, and deletes that isolated root. It
never registers `.mwfldemo` in the real per-user Classes store.

## 3. Consume installed components

```cmake
find_package(mwfl CONFIG REQUIRED COMPONENTS printing ole shell)
target_link_libraries(my_print_view PRIVATE mwfl::printing)
target_link_libraries(my_drop_window PRIVATE mwfl::ole)
target_link_libraries(my_shell_window PRIVATE mwfl::shell)
```

Use `mwfl::printing` for application-owned pagination and native print
transactions. Use `mwfl::ole` for COM data transfer and drag/drop. Use
`mwfl::shell` for versioned HKCU settings, reversible associations, Jump Lists,
recent documents, and taskbar state.

## 4. Safety rules

- Keep document content outside preview and printer resources.
- Let `PrintJob` balance native document/page transactions; cancellation aborts.
- Treat `STGMEDIUM`, HGLOBAL, COM references, and delayed callbacks as explicit
  ownership boundaries. Never put application pointers in transferred data.
- Register OLE targets and use taskbar objects only on their creating UI thread.
- Use stable application, task, ProgID, owner, and setting-schema identities.
- File association registration is per-user but still changes user state. Offer
  an explicit action and an equally visible removal action.
- On removal, delete only entries carrying your owner marker. Preserve unrelated
  registry values and honor removed Jump List tasks.
- Recreate taskbar state after the registered `TaskbarCreated` message.

## 5. Release verification

Repeat the build and tests in Release before accepting a change:

```powershell
cmake --build --preset vs2026-x64-release
ctest --preset vs2026-x64-release
```

During 0.4 through 0.8 this local VS2026/MSVC x64 Debug/Release matrix is the
approved feature gate. Other architectures, compilers, and remote workflows are
revalidated together after 0.8.
