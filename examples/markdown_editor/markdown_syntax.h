#pragma once

#include <mwfl/scintilla.h>

#include <filesystem>

namespace markdown_editor {

enum class EditorTheme { light, dark };

class MarkdownSyntax final {
public:
    MarkdownSyntax() noexcept = default;
    ~MarkdownSyntax() noexcept;
    MarkdownSyntax(const MarkdownSyntax&) = delete;
    MarkdownSyntax& operator=(const MarkdownSyntax&) = delete;

    bool LoadAdjacent() noexcept;
    bool Attach(mwfl::ScintillaEditor& editor) noexcept;
    void ApplyTheme(mwfl::ScintillaEditor& editor, EditorTheme theme) const noexcept;
    int StyleAt(const mwfl::ScintillaEditor& editor,
                mwfl::ScintillaPosition position) const noexcept;
    bool IsLoaded() const noexcept { return module_ != nullptr; }

private:
    HMODULE module_ = nullptr;
};

namespace markdown_style {
inline constexpr int strong = 2;
inline constexpr int emphasis = 4;
inline constexpr int header1 = 6;
inline constexpr int unordered_list = 13;
inline constexpr int block_quote = 15;
inline constexpr int link = 18;
inline constexpr int inline_code = 19;
inline constexpr int code_block = 21;
}  // namespace markdown_style

}  // namespace markdown_editor
