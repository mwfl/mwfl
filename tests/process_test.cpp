#include <mwfl/process.h>
#include <cassert>
int main() {
    assert(mwfl::QuoteWindowsArgument(L"plain") == L"plain");
    assert(mwfl::QuoteWindowsArgument(L"") == L"\"\"");
    assert(mwfl::QuoteWindowsArgument(L"a b") == L"\"a b\"");
    assert(mwfl::QuoteWindowsArgument(L"a\"b") == L"\"a\\\"b\"");
}
