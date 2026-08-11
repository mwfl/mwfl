# Hot Corners

This compiled example demonstrates **complete multi-monitor notification-area utility**. It is intentionally small
enough to study from the source while still using real native Windows controls,
messages, and ownership rules.

![Hot Corners example running on Windows](../../docs/images/examples/hot-corners.png)

## What it demonstrates

- `TrayIcon`
- `CommandSet`
- `DpiContext`

The window remains a real HWND and the example preserves mwfl's native escape
hatch. UI objects stay on their creating thread, and no exception is allowed to
cross a Win32 callback.

## Key code

The following excerpt is quoted directly from [`main.cpp`](main.cpp):

```cpp
class HotCornersWindow final : public WindowBase {
   public:
    void BuildUI() override {
        SetTitle(L"MWFL Hot Corners — multi-monitor utility");
        BuildMenu();
        CreateControls();
        RefreshMonitors();
        LoadSettings();
        PopulateSettingsControls();
        ApplyFont(GetDpiContext().GetDpi());
        if (!AddTrayIcon()) throw std::runtime_error("tray icon creation failed");
        if (!poll_.Start(*this, kPoll, 30ms))
            throw std::runtime_error("poll timer creation failed");
        if (g_self_test && !self_test_timer_.Start(*this, kSelfTest, 100ms))
            throw std::runtime_error("self-test timer creation failed");
        SavedWindowPlacement saved{};
        if (LoadWindowPlacementFromRegistry(HKEY_CURRENT_USER, kRegistryKey, L"MainWindow", saved))
            RestoreWindowPlacement(GetHwnd(), saved);
```

Read the complete implementation in [`hot_corner_model.cpp`](hot_corner_model.cpp), [`hot_corner_model.h`](hot_corner_model.h), [`main.cpp`](main.cpp).

## Build and run

From a Visual Studio developer PowerShell at the repository root:

```powershell
cmake --preset vs2026-x64
cmake --build --preset vs2026-x64-debug --target mwfl_hot_corners_demo
build\presets\vs2026-x64\examples\hot_corners\Debug\mwfl_hot_corners_demo.exe
```

Visual Studio 2022 remains supported; substitute the `vs2022-x64` configure and
build presets when validating that toolchain.

## Validation

The focused validation targets are `mwfl.tray_icon_native`, `mwfl.hot_corners_self_test`. Run `./scripts/verify-change.ps1 -Execute` after modifying it so
the repository selects the additional checks required by the changed paths.
