#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace markdown_editor {

enum class PreviewTheme { light, dark };

struct RenderOptions {
    PreviewTheme theme = PreviewTheme::light;
    std::filesystem::path document_directory;
};

// Produces a complete, offline HTML document. User input is escaped before it
// enters markup, and only http(s), mailto, document-relative, and fragment URLs
// are emitted. This intentionally small renderer is application-owned; mwfl
// remains a UI layer rather than a Markdown framework.
std::wstring RenderMarkdown(std::wstring_view markdown,
                            const RenderOptions& options = {});

}  // namespace markdown_editor
