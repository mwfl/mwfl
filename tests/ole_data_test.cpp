#include <mwfl/ole_data.h>

#include <shlobj_core.h>

#include <array>
#include <memory>

int main() {
    using namespace mwfl;
    const HRESULT initialized = ::OleInitialize(nullptr);
    if (FAILED(initialized)) return 1;
    struct Uninitialize { ~Uninitialize() { ::OleUninitialize(); } } uninitialize;

    const CLIPFORMAT custom = RegisterOleFormat(L"mwfl.tests.ole-data.v1");
    if (!custom || RegisterOleFormat(L"") != 0) return 2;
    OleDataObjectBuilder builder(4096);
    const std::array<std::filesystem::path, 2> files{L"C:\\one.txt", L"D:\\two.txt"};
    const auto calls = std::make_shared<int>(0);
    if (!builder.AddUnicodeText(L"Hello 中文") || !builder.AddFiles(files) ||
        !builder.AddDelayedBytes(custom, [calls] {
            ++*calls;
            return std::vector<std::byte>{std::byte{1}, std::byte{2}, std::byte{3}};
        }) || builder.AddUnicodeText(L"duplicate") || builder.GetFormatCount() != 3)
        return 3;
    auto object = builder.Build();
    if (!object) return 4;
    const auto text = ReadOleUnicodeText(*object.Get());
    const auto dropped = ReadOleFiles(*object.Get());
    const auto bytes = ReadOleBytes(*object.Get(), custom);
    if (!text || text.text != L"Hello 中文" || !dropped || dropped.files.size() != 2 ||
        dropped.files[1] != files[1] || !bytes || bytes.bytes.size() != 3 || *calls != 1)
        return 5;
    if (ReadOleBytes(*object.Get(), custom, 2).status != OleDataStatus::too_large ||
        *calls != 2)
        return 6;
    OleDataObjectBuilder bounded{8};
    const std::array<std::byte, 9> oversized{};
    if (bounded.AddBytes(custom, oversized) || bounded.GetFormatCount() != 0) return 17;

    Microsoft::WRL::ComPtr<IEnumFORMATETC> formats;
    if (FAILED(object->EnumFormatEtc(DATADIR_GET, &formats)) || !formats) return 7;
    FORMATETC first{};
    ULONG fetched = 0;
    if (formats->Next(1, &first, &fetched) != S_OK || fetched != 1 ||
        first.cfFormat != CF_UNICODETEXT)
        return 8;
    Microsoft::WRL::ComPtr<IEnumFORMATETC> clone;
    if (FAILED(formats->Clone(&clone)) || clone->Skip(1) != S_OK) return 9;
    FORMATETC third{};
    if (clone->Next(1, &third, nullptr) != S_OK || third.cfFormat != custom ||
        clone->Next(1, &third, nullptr) != S_FALSE || formats->Reset() != S_OK)
        return 10;

    FORMATETC unsupported{CF_BITMAP, nullptr, DVASPECT_CONTENT, -1, TYMED_GDI};
    if (object->QueryGetData(&unsupported) != DV_E_FORMATETC) return 11;
    OleDataObjectBuilder throwing;
    if (!throwing.AddDelayedBytes(custom, []() -> std::vector<std::byte> {
            throw 1;
        })) return 12;
    auto throwing_object = throwing.Build();
    if (ReadOleBytes(*throwing_object.Get(), custom).status != OleDataStatus::renderer_failure)
        return 13;

    OleDataObjectBuilder malformed_text;
    const std::array<std::byte, 3> odd_text{std::byte{1}, std::byte{2}, std::byte{3}};
    if (!malformed_text.AddBytes(CF_UNICODETEXT, odd_text) ||
        ReadOleUnicodeText(*malformed_text.Build().Get()).status != OleDataStatus::invalid_data)
        return 15;

    OleDataObjectBuilder malformed_files;
    std::vector<std::byte> file_payload(sizeof(DROPFILES) + 4 * sizeof(wchar_t));
    auto* drop = reinterpret_cast<DROPFILES*>(file_payload.data());
    drop->pFiles = sizeof(DROPFILES);
    drop->fWide = TRUE;
    auto* path = reinterpret_cast<wchar_t*>(file_payload.data() + sizeof(DROPFILES));
    path[0] = L'a'; path[1] = L'b'; path[2] = L'c'; path[3] = L'\0';
    if (!malformed_files.AddBytes(CF_HDROP, file_payload) ||
        ReadOleFiles(*malformed_files.Build().Get()).status != OleDataStatus::invalid_data)
        return 16;

    if (NegotiateDropEffect(DROPEFFECT_COPY | DROPEFFECT_MOVE, MK_SHIFT) != DROPEFFECT_MOVE ||
        NegotiateDropEffect(DROPEFFECT_COPY | DROPEFFECT_MOVE, MK_CONTROL) != DROPEFFECT_COPY ||
        NegotiateDropEffect(DROPEFFECT_LINK, 0, DROPEFFECT_MOVE) != DROPEFFECT_LINK ||
        NegotiateDropEffect(0, 0) != DROPEFFECT_NONE)
        return 14;
    return 0;
}
