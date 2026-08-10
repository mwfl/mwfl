#include <mwfl/ole_data.h>

#include <array>

Microsoft::WRL::ComPtr<IDataObject> BuildFixtureData() {
    const CLIPFORMAT format = mwfl::RegisterOleFormat(L"mwfl.agent.fixture.v1");
    const std::array payload{std::byte{0x01}, std::byte{0x02}};
    mwfl::OleDataObjectBuilder builder{1024};
    if (!format || !builder.AddUnicodeText(L"fixture") ||
        !builder.AddBytes(format, payload)) return {};
    return builder.Build();
}
