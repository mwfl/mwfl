#pragma once

#include <windows.h>

#include <exception>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>

namespace mwtl {

enum class HelpTargetKind { compiled_html, local_html, https_uri };

struct HelpRequest {
    HelpTargetKind kind = HelpTargetKind::compiled_html;
    std::filesystem::path local_path;
    std::wstring uri;
    std::wstring topic;
};

enum class HelpStatus {
    success,
    invalid_argument,
    untrusted_target,
    not_found,
    unavailable,
    cancelled,
    native_failure,
    callback_failed,
};

struct HelpResult {
    HelpStatus status = HelpStatus::invalid_argument;
    DWORD error = ERROR_INVALID_PARAMETER;
    std::exception_ptr exception{};
    explicit operator bool() const noexcept { return status == HelpStatus::success; }
};

// Validates without launching. Local targets must be absolute existing regular
// files with the matching extension. URI targets accept HTTPS only. Topic is a
// relative in-file fragment/path and rejects traversal, quotes, controls, and
// embedded NULs.
HelpResult ValidateHelpRequest(const HelpRequest& request) noexcept;

using HelpLaunchBackend = std::function<HelpResult(HWND, const HelpRequest&)>;

// The injected form enables deterministic offline testing and contains backend
// exceptions. The default backend calls HtmlHelpW for CHM and ShellExecuteExW
// for local HTML/HTTPS without constructing a command line or invoking a shell.
HelpResult LaunchHelpWithBackend(HWND owner, const HelpRequest& request,
                                 const HelpLaunchBackend& backend) noexcept;
HelpResult LaunchHelp(HWND owner, const HelpRequest& request) noexcept;

}  // namespace mwtl
