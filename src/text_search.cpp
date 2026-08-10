#include <mwtl/text_search.h>

#include <algorithm>
#include <cwctype>

namespace mwtl {
namespace {
bool IsWordCharacter(wchar_t value) noexcept {
    return std::iswalnum(value) != 0 || value == L'_';
}
}  // namespace

bool TextMatchesAt(std::wstring_view text, std::wstring_view query,
                   std::size_t offset, TextSearchOptions options) {
    if (query.empty() || offset + query.size() > text.size()) return false;
    for (std::size_t index = 0; index < query.size(); ++index) {
        const wchar_t left = text[offset + index];
        const wchar_t right = query[index];
        if (options.match_case ? left != right
                               : std::towlower(left) != std::towlower(right)) return false;
    }
    if (!options.whole_word) return true;
    return (offset == 0 || !IsWordCharacter(text[offset - 1])) &&
           (offset + query.size() == text.size() ||
            !IsWordCharacter(text[offset + query.size()]));
}

std::optional<std::size_t> FindTextMatch(std::wstring_view text,
                                         std::wstring_view query,
                                         std::size_t start,
                                         TextSearchOptions options) {
    if (query.empty()) return std::nullopt;
    if (!options.backwards) {
        for (std::size_t offset = (std::min)(start, text.size());
             offset + query.size() <= text.size(); ++offset)
            if (TextMatchesAt(text, query, offset, options)) return offset;
    } else {
        std::size_t offset = (std::min)(start, text.size());
        while (offset > 0) {
            --offset;
            if (TextMatchesAt(text, query, offset, options)) return offset;
        }
    }
    return std::nullopt;
}

std::size_t ReplaceAllText(std::wstring& text, std::wstring_view query,
                           std::wstring_view replacement,
                           TextSearchOptions options) {
    options.backwards = false;
    const std::wstring stable_query{query};
    const std::wstring stable_replacement{replacement};
    std::size_t offset{};
    std::size_t count{};
    while (const auto match = FindTextMatch(text, stable_query, offset, options)) {
        text.replace(*match, stable_query.size(), stable_replacement);
        offset = *match + stable_replacement.size();
        ++count;
    }
    return count;
}

}  // namespace mwtl
