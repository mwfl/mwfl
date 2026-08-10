#include <mwfl/scintilla.h>

#include <string>

int main() {
    const std::wstring source = L"Hello, 世界 👋";
    const auto utf8 = mwfl::ToUtf8(source);
    if (!utf8 || *utf8 != "Hello, \xE4\xB8\x96\xE7\x95\x8C \xF0\x9F\x91\x8B") return 1;
    const auto round_trip = mwfl::FromUtf8(*utf8);
    if (!round_trip || *round_trip != source) return 2;
    const std::string invalid{"\xC0\xAF", 2};
    if (mwfl::FromUtf8(invalid)) return 3;
    const wchar_t unpaired[] = {static_cast<wchar_t>(0xD800)};
    if (mwfl::ToUtf8(std::wstring_view(unpaired, 1))) return 4;
    if (!mwfl::ToUtf8(L"") || !mwfl::FromUtf8("")) return 5;
    return 0;
}
