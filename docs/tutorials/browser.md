# Build and extend the WebView2 Browser

This tutorial targets Windows 10 or later, Visual Studio 2026, MSVC C++20,
and x64. WebView2 is optional: a normal `mwfl::mwfl` build does not download
its SDK or require the Evergreen Runtime.

## 1. Build the reference application

From Developer PowerShell in the repository root:

```powershell
cmake --preset vs2026-x64-webview2
cmake --build --preset vs2026-x64-webview2-debug --target mwfl_browser_demo
./build/presets/vs2026-x64-webview2/examples/browser/Debug/mwfl_browser_demo.exe
```

The first configure downloads the official WebView2 SDK `1.0.4129.50` and
checks its pinned SHA-256. It does not install the browser Runtime. Windows 10
machines without the Evergreen Runtime get a structured `missing` result and
the application shows installation guidance instead of failing or opening an
installer.

## 2. Link an application

For an installed package:

```cmake
find_package(mwfl CONFIG REQUIRED COMPONENTS webview2)
add_executable(my_browser WIN32 main.cpp)
target_link_libraries(my_browser PRIVATE mwfl::webview2)
```

For `FetchContent` or `add_subdirectory`, set `MWFL_BUILD_WEBVIEW2=ON` before
making mwfl available, then link the same target. Projects that only link
`mwfl::mwfl` do not fetch or link WebView2.

## 3. Create and initialize the host

Store `WebView2Host` as a window member, create it through `ControlHost`, and
run the application in an STA:

```cpp
mwfl::WebView2Host browser_;

ui.AddNative(browser_, mwfl::ControlId{500}, mwfl::RectDip{});
browser_.Initialize({}, {
    .initialized = [this](mwfl::WebView2InitializationResult result) {
        if (result.state == mwfl::WebView2HostState::ready)
            browser_.NavigateToString(L"<h1>Offline welcome</h1>");
    },
    .process_failed = [this](mwfl::WebView2ProcessFailureKind) {
        PostMessageW(GetHwnd(), WM_APP + 1, 0, 0);
    }});
```

The callbacks and every host operation stay on the creating STA thread.
`Initialize` is asynchronous; do not navigate until the ready result. Post a
window message before calling `Restart` from a process-failure callback so
event teardown is not reentrant.

`WebView2Host` owns the controller and its event subscriptions. The environment,
controller, web-view escape-hatch pointers, and callback arguments are borrowed.
`Close` invalidates pending callbacks, removes events, closes the controller,
and is safe to repeat.

## 4. Resize, focus, and keyboard routing

Put the host in a retained layout like any native control. It updates controller
bounds on resize and parent DPI changes. When its container receives focus it
moves focus into web content. Use `accelerator_key` to hand application
shortcuts such as Ctrl+L back to the native address bar; return `true` only
when the application consumed the key.

## 5. Test without network or dialogs

```powershell
ctest --test-dir build/presets/vs2026-x64-webview2 -C Debug `
  -R "mwfl.(webview2_model|webview2_native|browser_gui)"
```

The tests classify missing Runtime deterministically, create a real controller
when available, navigate only to an in-memory HTML string, resize/focus/restart,
close during pending initialization, and terminate without user input. No test
depends on DNS or an external page.
