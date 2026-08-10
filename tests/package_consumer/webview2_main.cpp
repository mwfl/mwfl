#include <mwfl/webview2.h>

int main() {
    const auto missing = mwfl::ClassifyWebView2RuntimeResult(
        HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
    return missing.status == mwfl::WebView2RuntimeStatus::missing &&
                   mwfl::kWebView2SdkVersion == "1.0.4129.50"
               ? 0 : 1;
}
