#include <mwtl/ole_data.h>

int main() {
    mwtl::OleDataObjectBuilder builder;
    if (!builder.AddUnicodeText(L"package consumer")) return 1;
    auto object = builder.Build();
    const auto text = object ? mwtl::ReadOleUnicodeText(*object.Get()) : mwtl::OleTextResult{};
    return text && text.text == L"package consumer" ? 0 : 2;
}
