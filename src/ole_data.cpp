#include <mwtl/ole_data.h>

#include <shlobj_core.h>
#include <wrl/implements.h>

#include <algorithm>
#include <cstring>
#include <limits>
#include <memory>
#include <utility>

namespace mwtl {
namespace {

using Microsoft::WRL::ClassicCom;
using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Make;
using Microsoft::WRL::RuntimeClass;
using Microsoft::WRL::RuntimeClassFlags;

OleDataStatus MapError(HRESULT error) noexcept {
    if (error == DV_E_FORMATETC) return OleDataStatus::format_unavailable;
    if (error == E_OUTOFMEMORY) return OleDataStatus::allocation_failure;
    if (error == E_UNEXPECTED) return OleDataStatus::renderer_failure;
    if (error == HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE)) return OleDataStatus::too_large;
    if (error == E_INVALIDARG || error == STG_E_INVALIDHEADER)
        return OleDataStatus::invalid_data;
    return OleDataStatus::com_failure;
}

FORMATETC MakeFormat(CLIPFORMAT format) noexcept {
    return {format, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
}

bool Matches(const FORMATETC& requested, CLIPFORMAT format) noexcept {
    return requested.cfFormat == format && (requested.tymed & TYMED_HGLOBAL) != 0 &&
           requested.dwAspect == DVASPECT_CONTENT && requested.lindex == -1;
}

HRESULT CopyToGlobal(std::span<const std::byte> bytes, STGMEDIUM* medium) noexcept {
    if (!medium || bytes.empty()) return E_INVALIDARG;
    HGLOBAL memory = ::GlobalAlloc(GMEM_MOVEABLE, bytes.size());
    if (!memory) return E_OUTOFMEMORY;
    void* destination = ::GlobalLock(memory);
    if (!destination) {
        ::GlobalFree(memory);
        return E_OUTOFMEMORY;
    }
    std::memcpy(destination, bytes.data(), bytes.size());
    ::GlobalUnlock(memory);
    medium->tymed = TYMED_HGLOBAL;
    medium->hGlobal = memory;
    medium->pUnkForRelease = nullptr;
    return S_OK;
}

struct ObjectEntry {
    CLIPFORMAT format = 0;
    std::vector<std::byte> bytes;
    OleDelayedBytesRenderer renderer;
};

class FormatEnumerator final
    : public RuntimeClass<RuntimeClassFlags<ClassicCom>, IEnumFORMATETC> {
public:
    explicit FormatEnumerator(std::vector<FORMATETC> formats, ULONG index = 0)
        : formats_(std::move(formats)), index_(index) {}

    HRESULT STDMETHODCALLTYPE Next(ULONG count, FORMATETC* output,
                                   ULONG* fetched) noexcept override {
        if (!output || (count != 1 && !fetched)) return E_INVALIDARG;
        ULONG copied = 0;
        while (copied < count && index_ < formats_.size())
            output[copied++] = formats_[index_++];
        if (fetched) *fetched = copied;
        return copied == count ? S_OK : S_FALSE;
    }
    HRESULT STDMETHODCALLTYPE Skip(ULONG count) noexcept override {
        const std::size_t remaining = formats_.size() - index_;
        const std::size_t skipped = (std::min)(remaining, static_cast<std::size_t>(count));
        index_ += static_cast<ULONG>(skipped);
        return skipped == count ? S_OK : S_FALSE;
    }
    HRESULT STDMETHODCALLTYPE Reset() noexcept override { index_ = 0; return S_OK; }
    HRESULT STDMETHODCALLTYPE Clone(IEnumFORMATETC** output) noexcept override {
        if (!output) return E_POINTER;
        *output = nullptr;
        auto clone = Make<FormatEnumerator>(formats_, index_);
        if (!clone) return E_OUTOFMEMORY;
        *output = clone.Detach();
        return S_OK;
    }

private:
    std::vector<FORMATETC> formats_;
    ULONG index_ = 0;
};

class DataObject final : public RuntimeClass<RuntimeClassFlags<ClassicCom>, IDataObject> {
public:
    DataObject(std::vector<ObjectEntry> entries, std::size_t maximum_bytes)
        : entries_(std::move(entries)), maximum_bytes_(maximum_bytes) {}

    HRESULT STDMETHODCALLTYPE GetData(FORMATETC* format, STGMEDIUM* medium) noexcept override {
        if (!format || !medium) return E_INVALIDARG;
        *medium = {};
        const auto found = std::find_if(entries_.begin(), entries_.end(),
            [&](const ObjectEntry& entry) { return Matches(*format, entry.format); });
        if (found == entries_.end()) return DV_E_FORMATETC;
        if (!found->renderer) return CopyToGlobal(found->bytes, medium);
        try {
            const auto rendered = found->renderer();
            if (rendered.empty()) return E_FAIL;
            if (rendered.size() > maximum_bytes_)
                return HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE);
            return CopyToGlobal(rendered, medium);
        } catch (...) {
            return E_UNEXPECTED;
        }
    }
    HRESULT STDMETHODCALLTYPE GetDataHere(FORMATETC*, STGMEDIUM*) noexcept override {
        return DATA_E_FORMATETC;
    }
    HRESULT STDMETHODCALLTYPE QueryGetData(FORMATETC* format) noexcept override {
        if (!format) return E_INVALIDARG;
        return std::any_of(entries_.begin(), entries_.end(),
            [&](const ObjectEntry& entry) { return Matches(*format, entry.format); })
                   ? S_OK : DV_E_FORMATETC;
    }
    HRESULT STDMETHODCALLTYPE GetCanonicalFormatEtc(FORMATETC*, FORMATETC* output) noexcept override {
        if (!output) return E_POINTER;
        output->ptd = nullptr;
        return DATA_S_SAMEFORMATETC;
    }
    HRESULT STDMETHODCALLTYPE SetData(FORMATETC*, STGMEDIUM*, BOOL) noexcept override {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE EnumFormatEtc(DWORD direction,
                                             IEnumFORMATETC** output) noexcept override {
        if (!output) return E_POINTER;
        *output = nullptr;
        if (direction != DATADIR_GET) return E_NOTIMPL;
        std::vector<FORMATETC> formats;
        formats.reserve(entries_.size());
        for (const auto& entry : entries_) formats.push_back(MakeFormat(entry.format));
        auto enumerator = Make<FormatEnumerator>(std::move(formats));
        if (!enumerator) return E_OUTOFMEMORY;
        *output = enumerator.Detach();
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE DAdvise(FORMATETC*, DWORD, IAdviseSink*, DWORD*) noexcept override {
        return OLE_E_ADVISENOTSUPPORTED;
    }
    HRESULT STDMETHODCALLTYPE DUnadvise(DWORD) noexcept override { return OLE_E_ADVISENOTSUPPORTED; }
    HRESULT STDMETHODCALLTYPE EnumDAdvise(IEnumSTATDATA**) noexcept override {
        return OLE_E_ADVISENOTSUPPORTED;
    }

private:
    std::vector<ObjectEntry> entries_;
    std::size_t maximum_bytes_;
};

bool IsSingleEffect(DWORD effect) noexcept {
    effect &= DROPEFFECT_COPY | DROPEFFECT_MOVE | DROPEFFECT_LINK;
    return effect != 0 && (effect & (effect - 1)) == 0;
}

}  // namespace

OleDataObjectBuilder::OleDataObjectBuilder(std::size_t maximum_payload_bytes)
    : maximum_payload_bytes_(maximum_payload_bytes) {}

bool OleDataObjectBuilder::CanAdd(CLIPFORMAT format) const noexcept {
    return format != 0 && maximum_payload_bytes_ > 0 &&
           std::none_of(entries_.begin(), entries_.end(),
                        [format](const Entry& entry) { return entry.format == format; });
}

bool OleDataObjectBuilder::AddUnicodeText(std::wstring_view text) {
    if (text.find(L'\0') != std::wstring_view::npos) return false;
    const std::size_t characters = text.size() + 1;
    if (characters > (std::numeric_limits<std::size_t>::max)() / sizeof(wchar_t)) return false;
    const std::size_t bytes = characters * sizeof(wchar_t);
    if (!CanAdd(CF_UNICODETEXT) || bytes > maximum_payload_bytes_) return false;
    Entry entry;
    entry.format = CF_UNICODETEXT;
    entry.bytes.resize(bytes);
    std::memcpy(entry.bytes.data(), text.data(), text.size() * sizeof(wchar_t));
    entries_.push_back(std::move(entry));
    return true;
}

bool OleDataObjectBuilder::AddFiles(std::span<const std::filesystem::path> files) {
    if (!CanAdd(CF_HDROP) || files.empty()) return false;
    std::size_t characters = 1;
    for (const auto& file : files) {
        const std::wstring value = file.native();
        if (value.empty() || value.find(L'\0') != std::wstring::npos ||
            value.size() > 32767 || characters > (std::numeric_limits<std::size_t>::max)() -
                                                value.size() - 1)
            return false;
        characters += value.size() + 1;
    }
    if (characters > ((std::numeric_limits<std::size_t>::max)() - sizeof(DROPFILES)) /
                         sizeof(wchar_t))
        return false;
    const std::size_t bytes = sizeof(DROPFILES) + characters * sizeof(wchar_t);
    if (bytes > maximum_payload_bytes_) return false;
    Entry entry;
    entry.format = CF_HDROP;
    entry.bytes.resize(bytes);
    auto* drop = reinterpret_cast<DROPFILES*>(entry.bytes.data());
    drop->pFiles = sizeof(DROPFILES);
    drop->fWide = TRUE;
    auto* output = reinterpret_cast<wchar_t*>(entry.bytes.data() + sizeof(DROPFILES));
    for (const auto& file : files) {
        const std::wstring value = file.native();
        std::memcpy(output, value.c_str(), (value.size() + 1) * sizeof(wchar_t));
        output += value.size() + 1;
    }
    *output = L'\0';
    entries_.push_back(std::move(entry));
    return true;
}

bool OleDataObjectBuilder::AddBytes(CLIPFORMAT format, std::span<const std::byte> bytes) {
    if (!CanAdd(format) || bytes.empty() || bytes.size() > maximum_payload_bytes_) return false;
    Entry entry;
    entry.format = format;
    entry.bytes.assign(bytes.begin(), bytes.end());
    entries_.push_back(std::move(entry));
    return true;
}

bool OleDataObjectBuilder::AddDelayedBytes(CLIPFORMAT format,
                                           OleDelayedBytesRenderer renderer) {
    if (!CanAdd(format) || !renderer) return false;
    entries_.push_back({format, {}, std::move(renderer)});
    return true;
}

ComPtr<IDataObject> OleDataObjectBuilder::Build() const {
    std::vector<ObjectEntry> entries;
    entries.reserve(entries_.size());
    for (const auto& entry : entries_)
        entries.push_back({entry.format, entry.bytes, entry.renderer});
    return Make<DataObject>(std::move(entries), maximum_payload_bytes_);
}

std::size_t OleDataObjectBuilder::GetFormatCount() const noexcept { return entries_.size(); }

CLIPFORMAT RegisterOleFormat(std::wstring_view name) noexcept {
    if (name.empty() || name.find(L'\0') != std::wstring_view::npos) return 0;
    const std::wstring terminated(name);
    return static_cast<CLIPFORMAT>(::RegisterClipboardFormatW(terminated.c_str()));
}

OleBytesResult ReadOleBytes(IDataObject& object, CLIPFORMAT format,
                            std::size_t maximum_bytes) {
    OleBytesResult result;
    if (format == 0 || maximum_bytes == 0) {
        result.status = OleDataStatus::invalid_data;
        result.error = E_INVALIDARG;
        return result;
    }
    FORMATETC descriptor = MakeFormat(format);
    STGMEDIUM medium{};
    result.error = object.GetData(&descriptor, &medium);
    if (FAILED(result.error)) {
        result.status = MapError(result.error);
        return result;
    }
    struct Release { STGMEDIUM* value; ~Release() { ::ReleaseStgMedium(value); } } release{&medium};
    if (medium.tymed != TYMED_HGLOBAL || !medium.hGlobal) {
        result.status = OleDataStatus::invalid_data;
        result.error = DV_E_TYMED;
        return result;
    }
    const SIZE_T size = ::GlobalSize(medium.hGlobal);
    if (size == 0 || size > maximum_bytes) {
        result.status = size > maximum_bytes ? OleDataStatus::too_large : OleDataStatus::invalid_data;
        result.error = size > maximum_bytes ? HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE)
                                            : STG_E_INVALIDHEADER;
        return result;
    }
    const void* source = ::GlobalLock(medium.hGlobal);
    if (!source) {
        result.status = OleDataStatus::invalid_data;
        result.error = HRESULT_FROM_WIN32(::GetLastError());
        return result;
    }
    result.bytes.resize(size);
    std::memcpy(result.bytes.data(), source, size);
    ::GlobalUnlock(medium.hGlobal);
    result.status = OleDataStatus::success;
    result.error = S_OK;
    return result;
}

OleTextResult ReadOleUnicodeText(IDataObject& object, std::size_t maximum_characters) {
    OleTextResult result;
    if (maximum_characters == 0 || maximum_characters >
            (std::numeric_limits<std::size_t>::max)() / sizeof(wchar_t)) {
        result.status = OleDataStatus::invalid_data;
        result.error = E_INVALIDARG;
        return result;
    }
    auto bytes = ReadOleBytes(object, CF_UNICODETEXT,
                              maximum_characters * sizeof(wchar_t));
    result.status = bytes.status;
    result.error = bytes.error;
    if (!bytes) return result;
    if (bytes.bytes.size() % sizeof(wchar_t) != 0) {
        result.status = OleDataStatus::invalid_data;
        result.error = STG_E_INVALIDHEADER;
        return result;
    }
    const auto* text = reinterpret_cast<const wchar_t*>(bytes.bytes.data());
    const std::size_t count = bytes.bytes.size() / sizeof(wchar_t);
    const auto end = std::find(text, text + count, L'\0');
    if (end == text + count) {
        result.status = OleDataStatus::invalid_data;
        result.error = STG_E_INVALIDHEADER;
        return result;
    }
    result.text.assign(text, end);
    result.status = OleDataStatus::success;
    result.error = S_OK;
    return result;
}

OleFilesResult ReadOleFiles(IDataObject& object, std::size_t maximum_files,
                            std::size_t maximum_path_characters) {
    OleFilesResult result;
    if (maximum_files == 0 || maximum_path_characters == 0) {
        result.status = OleDataStatus::invalid_data;
        result.error = E_INVALIDARG;
        return result;
    }
    FORMATETC descriptor = MakeFormat(CF_HDROP);
    STGMEDIUM medium{};
    result.error = object.GetData(&descriptor, &medium);
    if (FAILED(result.error)) { result.status = MapError(result.error); return result; }
    struct Release { STGMEDIUM* value; ~Release() { ::ReleaseStgMedium(value); } } release{&medium};
    if (medium.tymed != TYMED_HGLOBAL || !medium.hGlobal) {
        result.status = OleDataStatus::invalid_data; result.error = DV_E_TYMED; return result;
    }
    const SIZE_T bytes = ::GlobalSize(medium.hGlobal);
    const auto* drop = static_cast<const DROPFILES*>(::GlobalLock(medium.hGlobal));
    if (!drop || bytes < sizeof(DROPFILES) || drop->pFiles < sizeof(DROPFILES) ||
        drop->pFiles >= bytes || !drop->fWide) {
        if (drop) ::GlobalUnlock(medium.hGlobal);
        result.status = OleDataStatus::invalid_data; result.error = STG_E_INVALIDHEADER; return result;
    }
    const auto* cursor = reinterpret_cast<const wchar_t*>(
        reinterpret_cast<const std::byte*>(drop) + drop->pFiles);
    const std::size_t remaining_bytes = bytes - drop->pFiles;
    if (remaining_bytes % sizeof(wchar_t) != 0) {
        ::GlobalUnlock(medium.hGlobal);
        result.status = OleDataStatus::invalid_data; result.error = STG_E_INVALIDHEADER; return result;
    }
    std::size_t remaining = remaining_bytes / sizeof(wchar_t);
    while (remaining > 0 && *cursor != L'\0') {
        if (result.files.size() >= maximum_files) {
            ::GlobalUnlock(medium.hGlobal);
            result.files.clear(); result.status = OleDataStatus::too_large;
            result.error = HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE); return result;
        }
        std::size_t length = 0;
        while (length < remaining && cursor[length] != L'\0') ++length;
        if (length == remaining || length > maximum_path_characters) {
            ::GlobalUnlock(medium.hGlobal);
            result.files.clear(); result.status = length > maximum_path_characters
                ? OleDataStatus::too_large : OleDataStatus::invalid_data;
            result.error = length > maximum_path_characters
                ? HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE) : STG_E_INVALIDHEADER;
            return result;
        }
        result.files.emplace_back(std::wstring(cursor, length));
        cursor += length + 1;
        remaining -= length + 1;
    }
    const bool list_terminated = remaining > 0 && *cursor == L'\0';
    ::GlobalUnlock(medium.hGlobal);
    if (result.files.empty() || !list_terminated) {
        result.status = OleDataStatus::invalid_data; result.error = STG_E_INVALIDHEADER; return result;
    }
    result.status = OleDataStatus::success; result.error = S_OK; return result;
}

DWORD NegotiateDropEffect(DWORD allowed, DWORD key_state, DWORD preferred) noexcept {
    allowed &= DROPEFFECT_COPY | DROPEFFECT_MOVE | DROPEFFECT_LINK;
    if (allowed == 0) return DROPEFFECT_NONE;
    DWORD requested = DROPEFFECT_NONE;
    if ((key_state & MK_CONTROL) && (key_state & MK_SHIFT)) requested = DROPEFFECT_LINK;
    else if (key_state & MK_CONTROL) requested = DROPEFFECT_COPY;
    else if (key_state & MK_SHIFT) requested = DROPEFFECT_MOVE;
    else if (IsSingleEffect(preferred)) requested = preferred;
    if ((requested & allowed) != 0) return requested;
    for (DWORD fallback : {DROPEFFECT_COPY, DROPEFFECT_MOVE, DROPEFFECT_LINK})
        if ((fallback & allowed) != 0) return fallback;
    return DROPEFFECT_NONE;
}

}  // namespace mwtl
