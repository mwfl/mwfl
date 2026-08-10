#pragma once

#include <windows.h>

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

#include <mwfl/async.h>
#include <mwfl/controls.h>

namespace mwfl {

inline constexpr std::string_view kWebView2SdkVersion = "1.0.4129.50";
inline constexpr std::string_view kWebView2SdkSha256 =
    "d3934f482d484b89fb4825df720c710664e1143a1e90f7b3a60794ef33f473d2";

enum class WebView2RuntimeStatus { available, missing, failed };
enum class WebView2HostState { empty, created, initializing, ready, failed, closed };
enum class WebView2ProcessFailureKind {
    browser_process_exited,
    render_process_exited,
    render_process_unresponsive,
    frame_render_process_exited,
    utility_process_exited,
    sandbox_helper_process_exited,
    unknown,
};

struct WebView2RuntimeInfo {
    WebView2RuntimeStatus status = WebView2RuntimeStatus::failed;
    HRESULT error = E_FAIL;
    std::wstring version;
};

struct WebView2InitializationResult {
    WebView2HostState state = WebView2HostState::empty;
    WebView2RuntimeStatus runtime = WebView2RuntimeStatus::failed;
    HRESULT error = E_FAIL;
};

struct WebView2AcceleratorKey {
    UINT message = 0;
    UINT virtual_key = 0;
    LPARAM lparam = 0;
};

struct WebView2HostOptions {
    std::filesystem::path user_data_folder;
    std::wstring initial_uri;
};

struct WebView2HostCallbacks {
    std::function<void(WebView2InitializationResult)> initialized;
    std::function<void(bool, HRESULT)> navigation_completed;
    std::function<void(WebView2ProcessFailureKind)> process_failed;
    std::function<bool(const WebView2AcceleratorKey&)> accelerator_key;
};

// Queries the Evergreen WebView2 Runtime through the pinned static loader.
// Missing runtime is a normal structured result and never throws or displays UI.
WebView2RuntimeInfo QueryWebView2Runtime() noexcept;
WebView2RuntimeInfo ClassifyWebView2RuntimeResult(
    HRESULT result, std::wstring version = {}) noexcept;

// HWND container for an asynchronously created WebView2 controller. All methods
// and callbacks belong to the creating STA thread. Close invalidates pending
// callbacks, removes subscriptions, and closes the controller deterministically.
// GetController/GetWebView return borrowed COM interface pointers as void* escape
// hatches so the public header does not expose or require the WebView2 SDK.
class WebView2Host final : public NativeControl {
public:
    WebView2Host();
    ~WebView2Host() noexcept;
    WebView2Host(const WebView2Host&) = delete;
    WebView2Host& operator=(const WebView2Host&) = delete;
    WebView2Host(WebView2Host&&) = delete;
    WebView2Host& operator=(WebView2Host&&) = delete;

    bool Create(HWND parent, ControlId id, RectDip bounds);
    template <WindowLike Parent>
    bool Create(const Parent& parent, ControlId id, RectDip bounds) {
        return Create(parent.GetHwnd(), id, bounds);
    }

    bool Initialize(WebView2HostOptions options = {}, WebView2HostCallbacks callbacks = {});
    bool Restart();
    void Close() noexcept;
    WebView2HostState GetState() const noexcept;
    WebView2InitializationResult GetInitializationResult() const noexcept;
    bool IsReady() const noexcept;
    bool Navigate(std::wstring_view uri) noexcept;
    bool NavigateToString(std::wstring_view html) noexcept;
    bool GoBack() noexcept;
    bool GoForward() noexcept;
    bool Reload() noexcept;
    bool Stop() noexcept;
    bool FocusContent() noexcept;
    bool Arrange() noexcept;
    void* GetController() const noexcept;
    void* GetWebView() const noexcept;

private:
    static LRESULT CALLBACK SubclassProcedure(HWND window, UINT message, WPARAM wparam,
                                              LPARAM lparam, UINT_PTR subclass_id,
                                              DWORD_PTR reference) noexcept;
    struct Implementation;
    std::unique_ptr<Implementation> implementation_;
};

}  // namespace mwfl
