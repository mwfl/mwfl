#include <mwfl/diagnostics.h>
#include <cassert>
int main() {
    const auto text = mwfl::FormatDiagnosticEvent({mwfl::EventLevel::Error, L"test", 42, {{L"public", L"yes"}, {L"token", L"hidden", mwfl::FieldSensitivity::Secret}}});
    assert(text.find(L"yes") != std::wstring::npos);
    assert(text.find(L"hidden") == std::wstring::npos);
    assert(text.find(L"<secret>") != std::wstring::npos);
}
