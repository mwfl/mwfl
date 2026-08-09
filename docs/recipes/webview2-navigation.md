# Add a WebView2 navigation command

Link the optional component and keep the host plus address box as window
members:

```cmake
find_package(mwtl CONFIG REQUIRED COMPONENTS webview2)
target_link_libraries(my_app PRIVATE mwtl::webview2)
```

Create the host first, then initialize it asynchronously. Disable or reject
navigation until `WebView2HostState::ready`:

```cpp
browser_.Initialize({}, {
    .initialized = [this](mwtl::WebView2InitializationResult result) {
        go_.SetEnabled(result.state == mwtl::WebView2HostState::ready);
    },
    .navigation_completed = [this](bool ok, HRESULT error) {
        status_.SetText(ok ? L"Navigation complete" : L"Navigation failed");
    }});

if (event.IsClicked(go_)) {
    std::wstring uri = address_.GetText();
    if (uri.find(L"://") == std::wstring::npos) uri = L"https://" + uri;
    if (!browser_.Navigate(uri)) status_.SetText(L"Browser is not ready");
}
```

Runtime absence is not a navigation error. Check the initialization result's
`runtime` field and show Evergreen Runtime installation guidance. For tests,
use `NavigateToString` with local HTML instead of relying on a network URL.

Handle application accelerators through `accelerator_key`; return true only
after consuming the key. On a process failure, post a native message and call
`Restart` from that later message rather than tearing down subscriptions inside
the WebView2 callback.
