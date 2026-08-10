#include <mwtl/webview2.h>

int main() {
    const auto missing = mwtl::ClassifyWebView2RuntimeResult(
        HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
    return missing.status == mwtl::WebView2RuntimeStatus::missing &&
                   mwtl::kWebView2SdkVersion == "1.0.4129.50"
               ? 0 : 1;
}
