#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace mwfl {

struct TextSearchOptions {
    bool match_case = false;
    bool whole_word = false;
    bool backwards = false;
};

bool TextMatchesAt(std::wstring_view text, std::wstring_view query,
                   std::size_t offset, TextSearchOptions options = {});
std::optional<std::size_t> FindTextMatch(std::wstring_view text,
                                         std::wstring_view query,
                                         std::size_t start,
                                         TextSearchOptions options = {});
std::size_t ReplaceAllText(std::wstring& text, std::wstring_view query,
                           std::wstring_view replacement,
                           TextSearchOptions options = {});

}  // namespace mwfl
