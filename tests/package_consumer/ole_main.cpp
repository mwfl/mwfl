#include <mwfl/ole_data.h>

int main() {
    mwfl::OleDataObjectBuilder builder;
    if (!builder.AddUnicodeText(L"package consumer")) return 1;
    auto object = builder.Build();
    const auto text = object ? mwfl::ReadOleUnicodeText(*object.Get()) : mwfl::OleTextResult{};
    return text && text.text == L"package consumer" ? 0 : 2;
}
