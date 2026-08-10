#include <mwfl/help.h>

#include <windows.h>

#include <filesystem>
#include <fstream>
#include <stdexcept>

int main() {
    using namespace mwfl;
    const auto root = std::filesystem::temp_directory_path();
    const auto html = root / (L"mwfl-help-" +
        std::to_wstring(::GetCurrentProcessId()) + L".html");
    std::error_code ignored;
    std::filesystem::remove(html, ignored);
    { std::ofstream file(html); file << "<!doctype html><title>Help</title>"; }

    HelpRequest local{HelpTargetKind::local_html, html, {}, L"#topic"};
    if (!ValidateHelpRequest(local)) return 1;
    int launched = 0;
    if (!LaunchHelpWithBackend(nullptr, local,
            [&](HWND, const HelpRequest& request) {
                ++launched;
                return HelpResult{request.local_path == html ? HelpStatus::success
                                                             : HelpStatus::native_failure};
            }) || launched != 1) return 2;
    auto cancelled = LaunchHelpWithBackend(nullptr, local,
        [](HWND, const HelpRequest&) {
            return HelpResult{HelpStatus::cancelled, ERROR_CANCELLED};
        });
    if (cancelled.status != HelpStatus::cancelled) return 3;
    auto failed = LaunchHelpWithBackend(nullptr, local,
        [](HWND, const HelpRequest&) -> HelpResult { throw std::runtime_error("help"); });
    if (failed.status != HelpStatus::callback_failed || !failed.exception) return 4;

    HelpRequest https{HelpTargetKind::https_uri, {}, L"https://example.invalid/help", L"#api"};
    if (!ValidateHelpRequest(https) ||
        ValidateHelpRequest({HelpTargetKind::https_uri, {}, L"http://example.com", {}}).status !=
            HelpStatus::untrusted_target ||
        ValidateHelpRequest({HelpTargetKind::https_uri, {}, L"https://example.com/a b", {}}).status !=
            HelpStatus::untrusted_target ||
        ValidateHelpRequest({HelpTargetKind::https_uri, {}, L"https://", {}}).status !=
            HelpStatus::untrusted_target ||
        ValidateHelpRequest({HelpTargetKind::https_uri, {}, L"https://user@example.com", {}}).status !=
            HelpStatus::untrusted_target) return 5;
    if (ValidateHelpRequest({HelpTargetKind::local_html, L"relative.html", {}, {}}).status !=
            HelpStatus::invalid_argument ||
        ValidateHelpRequest({HelpTargetKind::local_html, html, {}, L"../escape"}).status !=
            HelpStatus::untrusted_target ||
        ValidateHelpRequest({HelpTargetKind::compiled_html, html, {}, {}}).status !=
            HelpStatus::untrusted_target) return 6;
    auto missing = html;
    missing += L".missing.html";
    if (ValidateHelpRequest({HelpTargetKind::local_html, missing, {}, {}}).status !=
            HelpStatus::not_found) return 7;
    std::filesystem::remove(html, ignored);
    return 0;
}
