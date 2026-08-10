#include <mwtl/text_search.h>

#include <cstdlib>
#include <string>

int main() {
    using mwtl::FindTextMatch;
    const std::wstring text = L"one ONE stone one_one one";
    if (FindTextMatch(text, L"ONE", 0) != 0) return EXIT_FAILURE;
    if (FindTextMatch(text, L"ONE", 0, {.match_case = true}) != 4) return EXIT_FAILURE;
    if (FindTextMatch(text, L"one", 1, {.whole_word = true}) != 4) return EXIT_FAILURE;
    if (FindTextMatch(text, L"one", text.size(), {.whole_word = true, .backwards = true}) != 22)
        return EXIT_FAILURE;
    if (FindTextMatch(text, L"missing", 0) || FindTextMatch(text, L"", 0)) return EXIT_FAILURE;

    std::wstring replace = L"cat catalog CAT cat";
    if (mwtl::ReplaceAllText(replace, L"cat", L"dog", {.whole_word = true}) != 3 ||
        replace != L"dog catalog dog dog") return EXIT_FAILURE;
    std::wstring growth = L"a a";
    if (mwtl::ReplaceAllText(growth, L"a", L"aa") != 2 || growth != L"aa aa")
        return EXIT_FAILURE;
    std::wstring shrink = L"aaaa";
    if (mwtl::ReplaceAllText(shrink, L"aa", L"a") != 2 || shrink != L"aa")
        return EXIT_FAILURE;
    return EXIT_SUCCESS;
}
