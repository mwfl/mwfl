#include "markdown_syntax.h"

#include <array>

namespace markdown_editor {
namespace {

constexpr UINT kGetStyleAt = 2010;
constexpr UINT kStyleClearAll = 2050;
constexpr UINT kStyleSetFore = 2051;
constexpr UINT kStyleSetBack = 2052;
constexpr UINT kStyleSetBold = 2053;
constexpr UINT kStyleSetItalic = 2054;
constexpr UINT kStyleSetUnderline = 2059;
constexpr UINT kSetSelectionFore = 2067;
constexpr UINT kSetSelectionBack = 2068;
constexpr UINT kSetCaretFore = 2069;
constexpr UINT kColourise = 4003;
constexpr UINT kSetILexer = 4033;
constexpr int kStyleDefault = 32;
constexpr int kStyleLineNumber = 33;

using CreateLexer = void*(__stdcall*)(const char* name);

struct Palette {
    COLORREF foreground;
    COLORREF background;
    COLORREF muted;
    COLORREF heading;
    COLORREF heading_alt;
    COLORREF strong;
    COLORREF emphasis;
    COLORREF link;
    COLORREF code;
    COLORREF code_background;
    COLORREF list;
    COLORREF selection;
};

constexpr Palette kLight{RGB(36, 41, 47), RGB(255, 255, 255), RGB(87, 96, 106),
                         RGB(5, 80, 174), RGB(130, 80, 223), RGB(207, 34, 46),
                         RGB(130, 80, 223), RGB(9, 105, 218), RGB(17, 99, 41),
                         RGB(246, 248, 250), RGB(149, 56, 0), RGB(180, 213, 255)};
constexpr Palette kDark{RGB(201, 209, 217), RGB(13, 17, 23), RGB(139, 148, 158),
                        RGB(88, 166, 255), RGB(210, 168, 255), RGB(255, 123, 114),
                        RGB(210, 168, 255), RGB(88, 166, 255), RGB(126, 231, 135),
                        RGB(22, 27, 34), RGB(255, 166, 87), RGB(38, 79, 120)};

void SetColours(mwfl::ScintillaEditor& editor, int style, COLORREF foreground,
                COLORREF background) noexcept {
    editor.Send(kStyleSetFore, style, foreground);
    editor.Send(kStyleSetBack, style, background);
}

}  // namespace

MarkdownSyntax::~MarkdownSyntax() noexcept {
    if (module_) ::FreeLibrary(module_);
}

bool MarkdownSyntax::LoadAdjacent() noexcept {
    if (module_) return true;
    std::array<wchar_t, 32768> path{};
    const DWORD length = ::GetModuleFileNameW(nullptr, path.data(),
                                              static_cast<DWORD>(path.size()));
    if (length == 0 || length >= path.size()) return false;
    const auto dll = std::filesystem::path(std::wstring_view(path.data(), length)).parent_path() /
                     L"Lexilla.dll";
    module_ = ::LoadLibraryExW(dll.c_str(), nullptr,
                               LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    return module_ != nullptr;
}

bool MarkdownSyntax::Attach(mwfl::ScintillaEditor& editor) noexcept {
    if (!LoadAdjacent()) return false;
    const auto create = reinterpret_cast<CreateLexer>(::GetProcAddress(module_, "CreateLexer"));
    if (!create) return false;
    void* lexer = create("markdown");
    if (!lexer) return false;
    // SCI_SETILEXER transfers ownership of the lexer instance to Scintilla.
    editor.Send(kSetILexer, 0, reinterpret_cast<LPARAM>(lexer));
    editor.Send(kColourise, 0, -1);
    return true;
}

void MarkdownSyntax::ApplyTheme(mwfl::ScintillaEditor& editor, EditorTheme theme) const noexcept {
    const Palette& palette = theme == EditorTheme::dark ? kDark : kLight;
    SetColours(editor, kStyleDefault, palette.foreground, palette.background);
    editor.Send(kStyleClearAll);
    SetColours(editor, kStyleLineNumber, palette.muted, palette.background);

    for (const int style : {2, 3}) {
        SetColours(editor, style, palette.strong, palette.background);
        editor.Send(kStyleSetBold, style, 1);
    }
    for (const int style : {4, 5}) {
        SetColours(editor, style, palette.emphasis, palette.background);
        editor.Send(kStyleSetItalic, style, 1);
    }
    for (int style = 6; style <= 11; ++style) {
        SetColours(editor, style, style >= 9 ? palette.heading_alt : palette.heading,
                   palette.background);
        editor.Send(kStyleSetBold, style, 1);
    }
    for (const int style : {13, 14, 17}) {
        SetColours(editor, style, palette.list, palette.background);
        editor.Send(kStyleSetBold, style, 1);
    }
    SetColours(editor, 15, palette.muted, palette.background);
    editor.Send(kStyleSetItalic, 15, 1);
    SetColours(editor, 16, palette.muted, palette.code_background);
    SetColours(editor, 18, palette.link, palette.background);
    editor.Send(kStyleSetUnderline, 18, 1);
    for (const int style : {19, 20, 21})
        SetColours(editor, style, palette.code, palette.code_background);

    editor.Send(kSetCaretFore, palette.foreground);
    editor.Send(kSetSelectionFore, 1, palette.foreground);
    editor.Send(kSetSelectionBack, 1, palette.selection);
    editor.Send(kColourise, 0, -1);
    ::InvalidateRect(editor.GetHwnd(), nullptr, FALSE);
}

int MarkdownSyntax::StyleAt(const mwfl::ScintillaEditor& editor,
                            mwfl::ScintillaPosition position) const noexcept {
    return static_cast<int>(editor.Send(kGetStyleAt, static_cast<WPARAM>(position)));
}

}  // namespace markdown_editor
