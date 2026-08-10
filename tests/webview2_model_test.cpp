#include <mwfl/webview2.h>

int main() {
    const auto available = mwfl::ClassifyWebView2RuntimeResult(S_OK, L"140.0.0.0");
    if (available.status != mwfl::WebView2RuntimeStatus::available ||
        available.error != S_OK || available.version != L"140.0.0.0") return 1;

    const HRESULT missing_file = HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
    const auto missing = mwfl::ClassifyWebView2RuntimeResult(missing_file, L"ignored");
    if (missing.status != mwfl::WebView2RuntimeStatus::missing ||
        missing.error != missing_file || !missing.version.empty()) return 2;

    const auto missing_path = mwfl::ClassifyWebView2RuntimeResult(
        HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND));
    if (missing_path.status != mwfl::WebView2RuntimeStatus::missing) return 3;

    const auto failed = mwfl::ClassifyWebView2RuntimeResult(E_ACCESSDENIED, L"ignored");
    if (failed.status != mwfl::WebView2RuntimeStatus::failed ||
        failed.error != E_ACCESSDENIED || !failed.version.empty()) return 4;
    return 0;
}
