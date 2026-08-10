#include "markdown_renderer.h"

#include <cstdlib>
#include <string>

namespace {

bool Contains(std::wstring_view value, std::wstring_view expected) {
    return value.find(expected) != std::wstring_view::npos;
}

}  // namespace

int main() {
    using markdown_editor::RenderMarkdown;

    const auto document = RenderMarkdown(
        L"# Heading\n\nA **strong** and *quiet* [link](https://example.com).\n\n"
        L"- [x] done\n- item\n\n> quote\n\n~~removed~~\n\n"
        L"| Name | Value |\n| --- | --- |\n| mwfl | native |\n\n"
        L"```html\n<b>source</b>\n```\n");
    if (!Contains(document, L"<h1>Heading</h1>")) return 1;
    if (!Contains(document, L"<strong>strong</strong>")) return 2;
    if (!Contains(document, L"<em>quiet</em>")) return 3;
    if (!Contains(document, L"href=\"https://example.com\"")) return 4;
    if (!Contains(document, L"type=\"checkbox\"") || !Contains(document, L"checked")) return 5;
    if (!Contains(document, L"&lt;b&gt;source&lt;/b&gt;")) return 6;
    if (!Contains(document, L"<del>removed</del>") || !Contains(document, L"<table>")) return 10;

    const auto hostile = RenderMarkdown(
        L"<script>alert('x')</script>\n\n"
        L"[bad](javascript:alert(1)) ![bad](data:text/html,payload)\n");
    if (Contains(hostile, L"<script>") || Contains(hostile, L"href=\"javascript:") ||
        Contains(hostile, L"src=\"data:text/html") ||
        !Contains(hostile, L"&lt;script&gt;")) return 7;

    const auto dark = RenderMarkdown(L"text", {.theme = markdown_editor::PreviewTheme::dark});
    if (!Contains(dark, L"--bg:#1e1f22")) return 8;
    const auto relative_image = RenderMarkdown(
        L"![diagram](assets/diagram.png)",
        {.document_directory = std::filesystem::path{L"C:\\Docs and Notes"}});
    if (!Contains(relative_image, L"<base href=\"file:///C:/Docs%20and%20Notes/\">") ||
        !Contains(relative_image, L"src=\"assets/diagram.png\"")) return 9;
    return EXIT_SUCCESS;
}
