#include "markdown_renderer.h"

#include <windows.h>

#include <md4c-html.h>

#include <algorithm>
#include <cwctype>
#include <string>
#include <string_view>

namespace markdown_editor {
namespace {

std::string ToUtf8(std::wstring_view text) {
    if (text.empty()) return {};
    const int needed = ::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(),
                                              static_cast<int>(text.size()), nullptr, 0,
                                              nullptr, nullptr);
    if (needed <= 0) return {};
    std::string result(static_cast<std::size_t>(needed), '\0');
    if (::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(),
                              static_cast<int>(text.size()), result.data(), needed,
                              nullptr, nullptr) != needed) return {};
    return result;
}

std::wstring FromUtf8(std::string_view text) {
    if (text.empty()) return {};
    const int needed = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                                              static_cast<int>(text.size()), nullptr, 0);
    if (needed <= 0) return {};
    std::wstring result(static_cast<std::size_t>(needed), L'\0');
    if (::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                              static_cast<int>(text.size()), result.data(), needed) != needed)
        return {};
    return result;
}

std::wstring EscapeAttribute(std::wstring_view text) {
    std::wstring result;
    for (const wchar_t value : text) {
        switch (value) {
            case L'&': result += L"&amp;"; break;
            case L'<': result += L"&lt;"; break;
            case L'>': result += L"&gt;"; break;
            case L'\"': result += L"&quot;"; break;
            case L'\'': result += L"&#39;"; break;
            default: result += value; break;
        }
    }
    return result;
}

bool StartsWithInsensitive(std::wstring_view value, std::wstring_view prefix) {
    return value.size() >= prefix.size() &&
           std::equal(prefix.begin(), prefix.end(), value.begin(),
                      [](wchar_t left, wchar_t right) {
                          return std::towlower(left) == std::towlower(right);
                      });
}

bool IsSafeUrl(std::wstring_view value) {
    if (value.empty() || value.front() == L'#') return true;
    if (StartsWithInsensitive(value, L"https://") ||
        StartsWithInsensitive(value, L"http://") ||
        StartsWithInsensitive(value, L"mailto:")) return true;
    const auto colon = value.find(L':');
    const auto slash = value.find_first_of(L"/\\");
    return colon == std::wstring_view::npos ||
           (slash != std::wstring_view::npos && slash < colon);
}

void FilterUrlAttributes(std::wstring& html) {
    for (const auto attribute : {std::wstring_view{L"href=\""},
                                 std::wstring_view{L"src=\""}}) {
        std::size_t search = 0;
        while ((search = html.find(attribute, search)) != std::wstring::npos) {
            const auto value_start = search + attribute.size();
            const auto value_end = html.find(L'\"', value_start);
            if (value_end == std::wstring::npos) break;
            const auto value = std::wstring_view{html}.substr(value_start,
                                                               value_end - value_start);
            if (!IsSafeUrl(value)) html.replace(value_start, value.size(), L"#blocked-url");
            search = value_end + 1;
        }
    }
}

std::wstring DocumentBase(const std::filesystem::path& directory) {
    if (directory.empty()) return {};
    std::error_code error;
    auto absolute = std::filesystem::absolute(directory, error);
    if (error) return {};
    std::wstring path = absolute.generic_wstring();
    std::wstring encoded;
    encoded.reserve(path.size());
    for (const wchar_t value : path) {
        if (value == L' ') encoded += L"%20";
        else if (value == L'#') encoded += L"%23";
        else if (value == L'%') encoded += L"%25";
        else encoded += value;
    }
    if (!encoded.ends_with(L'/')) encoded += L'/';
    return L"<base href=\"file:///" + EscapeAttribute(encoded) + L"\">";
}

void AppendHtml(const MD_CHAR* text, MD_SIZE size, void* context) {
    static_cast<std::string*>(context)->append(text, size);
}

std::wstring RenderBody(std::wstring_view markdown) {
    const std::string source = ToUtf8(markdown);
    if (!markdown.empty() && source.empty()) return L"<p>Invalid Unicode input.</p>";
    std::string html;
    const unsigned parser_flags = MD_DIALECT_GITHUB | MD_FLAG_NOHTMLBLOCKS |
                                  MD_FLAG_NOHTMLSPANS;
    if (md_html(source.data(), static_cast<MD_SIZE>(source.size()), AppendHtml, &html,
                parser_flags, MD_HTML_FLAG_SKIP_UTF8_BOM) != 0)
        return L"<p>Markdown rendering failed. The source document is unchanged.</p>";
    std::wstring result = FromUtf8(html);
    FilterUrlAttributes(result);
    return result;
}

}  // namespace

std::wstring RenderMarkdown(std::wstring_view markdown, const RenderOptions& options) {
    const bool dark = options.theme == PreviewTheme::dark;
    const std::wstring colors = dark
        ? L"--bg:#1e1f22;--fg:#d8d8d8;--muted:#9da1a6;--line:#3a3d41;--code:#292b2f;--accent:#6ea8fe;"
        : L"--bg:#fff;--fg:#24292f;--muted:#57606a;--line:#d0d7de;--code:#f6f8fa;--accent:#0969da;";
    return L"<!doctype html><html><head><meta charset=\"utf-8\">" +
           DocumentBase(options.document_directory) +
           L"<meta http-equiv=\"Content-Security-Policy\" content=\"default-src 'none'; img-src file: data: https: http:; style-src 'unsafe-inline';\">"
           L"<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><style>:root{" + colors +
           L"}*{box-sizing:border-box}body{margin:0 auto;padding:40px 46px 80px;max-width:900px;background:var(--bg);color:var(--fg);font:16px/1.65 'Segoe UI',sans-serif}"
           L"h1,h2{border-bottom:1px solid var(--line);padding-bottom:.3em}h1,h2,h3,h4{line-height:1.25;margin:1.5em 0 .6em}a{color:var(--accent)}"
           L"code,pre{font-family:'Cascadia Mono',Consolas,monospace;background:var(--code)}code{padding:.15em .35em;border-radius:4px}pre{padding:16px;overflow:auto;border-radius:7px}"
           L"pre code{padding:0}blockquote{margin-left:0;padding:.2em 1em;color:var(--muted);border-left:4px solid var(--line)}img{max-width:100%}hr{border:0;border-top:1px solid var(--line)}"
           L"table{border-spacing:0;border-collapse:collapse;display:block;max-width:100%;overflow:auto}th,td{padding:6px 13px;border:1px solid var(--line)}tr:nth-child(2n){background:var(--code)}"
           L".task-list-item{list-style:none}.task-list-item input{margin:0 .45em 0 -1.4em}</style></head><body>" +
           RenderBody(markdown) + L"</body></html>";
}

}  // namespace markdown_editor
