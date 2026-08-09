#include <mwtl/printing_settings.h>

#include <commdlg.h>
#include <winspool.h>

#include <cstring>
#include <limits>
#include <utility>
#include <vector>

namespace mwtl {
namespace detail {
PrintBackendResult AdoptPrinterDeviceContext(HDC dc);
}
namespace {

class PrinterHandle final {
public:
    ~PrinterHandle() { if (value) ::ClosePrinter(value); }
    HANDLE value = nullptr;
};

std::wstring Copy(const wchar_t* value) { return value ? value : L""; }

OwnedGlobalMemory CreateDevNames(const PRINTER_INFO_2W& info, bool is_default) {
    const std::wstring driver = Copy(info.pDriverName);
    const std::wstring device = Copy(info.pPrinterName);
    const std::wstring output = Copy(info.pPortName);
    const std::size_t header_chars = sizeof(DEVNAMES) / sizeof(wchar_t);
    const std::size_t total_chars = header_chars + driver.size() + 1 + device.size() + 1 +
                                    output.size() + 1;
    if (total_chars > (std::numeric_limits<WORD>::max)()) return {};
    auto memory = OwnedGlobalMemory::Allocate(total_chars * sizeof(wchar_t));
    auto lock = memory.Lock();
    if (!lock) return {};
    auto* names = lock.As<DEVNAMES>();
    WORD offset = static_cast<WORD>(header_chars);
    names->wDriverOffset = offset;
    auto* characters = reinterpret_cast<wchar_t*>(names);
    std::memcpy(characters + offset, driver.c_str(), (driver.size() + 1) * sizeof(wchar_t));
    offset = static_cast<WORD>(offset + driver.size() + 1);
    names->wDeviceOffset = offset;
    std::memcpy(characters + offset, device.c_str(), (device.size() + 1) * sizeof(wchar_t));
    offset = static_cast<WORD>(offset + device.size() + 1);
    names->wOutputOffset = offset;
    std::memcpy(characters + offset, output.c_str(), (output.size() + 1) * sizeof(wchar_t));
    names->wDefault = is_default ? DN_DEFAULTPRN : 0;
    return memory;
}

std::wstring PrinterNameFromDevNames(const OwnedGlobalMemory& memory) {
    auto lock = memory.Lock();
    if (!lock || memory.Size() < sizeof(DEVNAMES)) return {};
    const auto* names = lock.As<const DEVNAMES>();
    const std::size_t chars = memory.Size() / sizeof(wchar_t);
    if (names->wDeviceOffset >= chars) return {};
    const auto* base = reinterpret_cast<const wchar_t*>(names);
    const auto* value = base + names->wDeviceOffset;
    const std::size_t remaining = chars - names->wDeviceOffset;
    for (std::size_t index = 0; index < remaining; ++index)
        if (value[index] == L'\0') return std::wstring(value, index);
    return {};
}

}  // namespace

LockedGlobalMemory::LockedGlobalMemory(HGLOBAL memory) noexcept : memory_(memory) {
    if (memory_) address_ = ::GlobalLock(memory_);
    if (!address_) memory_ = nullptr;
}

LockedGlobalMemory::~LockedGlobalMemory() { Reset(); }

LockedGlobalMemory::LockedGlobalMemory(LockedGlobalMemory&& other) noexcept
    : memory_(std::exchange(other.memory_, nullptr)),
      address_(std::exchange(other.address_, nullptr)) {}

LockedGlobalMemory& LockedGlobalMemory::operator=(LockedGlobalMemory&& other) noexcept {
    if (this != &other) {
        Reset();
        memory_ = std::exchange(other.memory_, nullptr);
        address_ = std::exchange(other.address_, nullptr);
    }
    return *this;
}

void LockedGlobalMemory::Reset() noexcept {
    if (memory_) ::GlobalUnlock(memory_);
    memory_ = nullptr;
    address_ = nullptr;
}

OwnedGlobalMemory::~OwnedGlobalMemory() { Reset(); }

OwnedGlobalMemory::OwnedGlobalMemory(OwnedGlobalMemory&& other) noexcept
    : memory_(other.Release()) {}

OwnedGlobalMemory& OwnedGlobalMemory::operator=(OwnedGlobalMemory&& other) noexcept {
    if (this != &other) Reset(other.Release());
    return *this;
}

OwnedGlobalMemory OwnedGlobalMemory::Allocate(std::size_t bytes) noexcept {
    if (bytes == 0) return {};
    return OwnedGlobalMemory(::GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, bytes));
}

OwnedGlobalMemory OwnedGlobalMemory::Adopt(HGLOBAL memory) noexcept {
    return OwnedGlobalMemory(memory);
}

OwnedGlobalMemory OwnedGlobalMemory::Clone() const noexcept {
    const std::size_t size = Size();
    if (!memory_ || size == 0) return {};
    auto clone = Allocate(size);
    auto source = Lock();
    auto destination = clone.Lock();
    if (!source || !destination) return {};
    std::memcpy(destination.Get(), source.Get(), size);
    return clone;
}

HGLOBAL OwnedGlobalMemory::Release() noexcept { return std::exchange(memory_, nullptr); }

std::size_t OwnedGlobalMemory::Size() const noexcept {
    return memory_ ? ::GlobalSize(memory_) : 0;
}

void OwnedGlobalMemory::Reset(HGLOBAL memory) noexcept {
    if (memory_) ::GlobalFree(memory_);
    memory_ = memory;
}

PrinterSettings::PrinterSettings(std::wstring printer_name, OwnedGlobalMemory dev_mode,
                                 OwnedGlobalMemory dev_names) noexcept
    : printer_name_(std::move(printer_name)), dev_mode_(std::move(dev_mode)),
      dev_names_(std::move(dev_names)) {}

PrinterSettings PrinterSettings::Clone() const noexcept {
    if (!IsValid()) return {};
    auto mode = dev_mode_.Clone();
    auto names = dev_names_.Clone();
    if (!mode || !names) return {};
    return PrinterSettings(printer_name_, std::move(mode), std::move(names));
}

bool PrinterSettings::IsValid() const noexcept {
    if (printer_name_.empty() || dev_mode_.Size() < sizeof(DEVMODEW) ||
        dev_names_.Size() < sizeof(DEVNAMES))
        return false;
    auto mode = dev_mode_.Lock();
    auto names_lock = dev_names_.Lock();
    if (!mode || !names_lock) return false;
    const auto* native_mode = mode.As<const DEVMODEW>();
    const std::size_t mode_size = dev_mode_.Size();
    if (native_mode->dmSize < sizeof(DEVMODEW) || native_mode->dmSize > mode_size ||
        native_mode->dmDriverExtra > mode_size - native_mode->dmSize)
        return false;
    const auto* names = names_lock.As<const DEVNAMES>();
    const auto* characters = reinterpret_cast<const wchar_t*>(names);
    const std::size_t character_count = dev_names_.Size() / sizeof(wchar_t);
    for (WORD offset : {names->wDriverOffset, names->wDeviceOffset, names->wOutputOffset}) {
        if (offset < sizeof(DEVNAMES) / sizeof(wchar_t) || offset >= character_count)
            return false;
        bool terminated = false;
        for (std::size_t index = offset; index < character_count; ++index) {
            if (characters[index] == L'\0') { terminated = true; break; }
        }
        if (!terminated) return false;
    }
    return true;
}

PrinterSettingsResult LoadPrinterSettings(std::wstring_view printer_name) {
    PrinterSettingsResult result;
    if (printer_name.empty()) {
        result.status = PrintOperationStatus::invalid_settings;
        result.error = ERROR_INVALID_PARAMETER;
        return result;
    }
    const std::wstring name(printer_name);
    PrinterHandle printer;
    if (!::OpenPrinterW(const_cast<wchar_t*>(name.c_str()), &printer.value, nullptr)) {
        result.error = ::GetLastError();
        result.status = result.error == ERROR_INVALID_PRINTER_NAME
                            ? PrintOperationStatus::no_printer
                            : PrintOperationStatus::native_failure;
        return result;
    }
    const LONG mode_bytes = ::DocumentPropertiesW(nullptr, printer.value,
        const_cast<wchar_t*>(name.c_str()), nullptr, nullptr, 0);
    if (mode_bytes < static_cast<LONG>(sizeof(DEVMODEW))) {
        result.error = mode_bytes < 0 ? ::GetLastError() : ERROR_INVALID_DATA;
        result.status = PrintOperationStatus::invalid_settings;
        return result;
    }
    auto mode = OwnedGlobalMemory::Allocate(static_cast<std::size_t>(mode_bytes));
    auto mode_lock = mode.Lock();
    if (!mode_lock) {
        result.error = ERROR_OUTOFMEMORY;
        return result;
    }
    if (::DocumentPropertiesW(nullptr, printer.value, const_cast<wchar_t*>(name.c_str()),
                              mode_lock.As<DEVMODEW>(), nullptr, DM_OUT_BUFFER) != IDOK) {
        result.error = ::GetLastError();
        if (result.error == ERROR_SUCCESS) result.error = ERROR_INVALID_DATA;
        result.status = PrintOperationStatus::invalid_settings;
        return result;
    }
    DWORD bytes = 0;
    ::GetPrinterW(printer.value, 2, nullptr, 0, &bytes);
    if (bytes == 0) {
        result.error = ::GetLastError();
        return result;
    }
    std::vector<std::byte> info_buffer(bytes);
    if (!::GetPrinterW(printer.value, 2, reinterpret_cast<BYTE*>(info_buffer.data()), bytes,
                       &bytes)) {
        result.error = ::GetLastError();
        return result;
    }
    const auto default_printer = QueryDefaultPrinter();
    const auto& info = *reinterpret_cast<const PRINTER_INFO_2W*>(info_buffer.data());
    auto names = CreateDevNames(info, default_printer && default_printer.name == name);
    if (!names) {
        result.error = ERROR_INVALID_DATA;
        result.status = PrintOperationStatus::invalid_settings;
        return result;
    }
    result.settings = PrinterSettings(name, std::move(mode), std::move(names));
    result.status = PrintOperationStatus::success;
    result.error = ERROR_SUCCESS;
    return result;
}

PrintBackendResult CreatePrinterBackend(const PrinterSettings& settings) {
    PrintBackendResult result;
    if (!settings.IsValid()) {
        result.status = PrintOperationStatus::invalid_settings;
        result.error = ERROR_INVALID_DATA;
        return result;
    }
    auto mode = settings.LockDevMode();
    if (!mode) {
        result.status = PrintOperationStatus::invalid_settings;
        result.error = ERROR_INVALID_HANDLE;
        return result;
    }
    const std::wstring& name = settings.PrinterName();
    HDC dc = ::CreateDCW(L"WINSPOOL", name.c_str(), nullptr, mode.As<const DEVMODEW>());
    if (!dc) {
        result.error = ::GetLastError();
        if (result.error == ERROR_SUCCESS) result.error = ERROR_GEN_FAILURE;
        result.status = PrintOperationStatus::native_failure;
        return result;
    }
    return detail::AdoptPrinterDeviceContext(dc);
}

PrintDialogResult ShowPrintDialog(HWND owner, PrinterSettings settings, DWORD flags) noexcept {
    PrintDialogResult result;
    PRINTDLGEXW dialog{sizeof(dialog)};
    dialog.hwndOwner = owner;
    dialog.hDevMode = settings.dev_mode_.Release();
    dialog.hDevNames = settings.dev_names_.Release();
    dialog.Flags = flags | PD_RETURNDC | PD_USEDEVMODECOPIESANDCOLLATE | PD_NOCURRENTPAGE;
    dialog.nStartPage = START_PAGE_GENERAL;
    const HRESULT hr = ::PrintDlgExW(&dialog);
    result.settings.dev_mode_ = OwnedGlobalMemory::Adopt(dialog.hDevMode);
    result.settings.dev_names_ = OwnedGlobalMemory::Adopt(dialog.hDevNames);
    result.settings.printer_name_ = PrinterNameFromDevNames(result.settings.dev_names_);
    result.flags = dialog.Flags;
    result.copies = dialog.nCopies;
    if (dialog.hDC) ::DeleteDC(dialog.hDC);
    if (FAILED(hr)) {
        result.error = static_cast<DWORD>(hr);
        result.action = PrintDialogAction::failed;
    } else if (dialog.dwResultAction == PD_RESULT_CANCEL) {
        result.error = ERROR_CANCELLED;
        result.action = PrintDialogAction::cancelled;
    } else if (dialog.dwResultAction == PD_RESULT_PRINT && result.settings.IsValid()) {
        result.error = ERROR_SUCCESS;
        result.action = PrintDialogAction::accepted;
    } else {
        result.error = ERROR_INVALID_DATA;
        result.action = PrintDialogAction::failed;
    }
    return result;
}

}  // namespace mwtl
