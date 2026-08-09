#pragma once

#include <windows.h>

#include <cstddef>
#include <string>
#include <string_view>

#include <mwtl/printing_native.h>

namespace mwtl {

class PrinterSettings;
struct PrintDialogResult;
PrintDialogResult ShowPrintDialog(HWND owner, PrinterSettings settings,
                                  DWORD flags) noexcept;

class LockedGlobalMemory final {
public:
    LockedGlobalMemory() noexcept = default;
    explicit LockedGlobalMemory(HGLOBAL memory) noexcept;
    ~LockedGlobalMemory();
    LockedGlobalMemory(const LockedGlobalMemory&) = delete;
    LockedGlobalMemory& operator=(const LockedGlobalMemory&) = delete;
    LockedGlobalMemory(LockedGlobalMemory&& other) noexcept;
    LockedGlobalMemory& operator=(LockedGlobalMemory&& other) noexcept;

    void* Get() const noexcept { return address_; }
    template <class T> T* As() const noexcept { return static_cast<T*>(address_); }
    explicit operator bool() const noexcept { return address_ != nullptr; }

private:
    void Reset() noexcept;
    HGLOBAL memory_ = nullptr;
    void* address_ = nullptr;
};

// Move-only owner for GMEM_MOVEABLE memory used by common print dialogs.
class OwnedGlobalMemory final {
public:
    OwnedGlobalMemory() noexcept = default;
    ~OwnedGlobalMemory();
    OwnedGlobalMemory(const OwnedGlobalMemory&) = delete;
    OwnedGlobalMemory& operator=(const OwnedGlobalMemory&) = delete;
    OwnedGlobalMemory(OwnedGlobalMemory&& other) noexcept;
    OwnedGlobalMemory& operator=(OwnedGlobalMemory&& other) noexcept;

    static OwnedGlobalMemory Allocate(std::size_t bytes) noexcept;
    static OwnedGlobalMemory Adopt(HGLOBAL memory) noexcept;
    OwnedGlobalMemory Clone() const noexcept;
    LockedGlobalMemory Lock() const noexcept { return LockedGlobalMemory(memory_); }
    HGLOBAL Get() const noexcept { return memory_; }
    HGLOBAL Release() noexcept;
    std::size_t Size() const noexcept;
    explicit operator bool() const noexcept { return memory_ != nullptr; }

private:
    explicit OwnedGlobalMemory(HGLOBAL memory) noexcept : memory_(memory) {}
    void Reset(HGLOBAL memory = nullptr) noexcept;
    HGLOBAL memory_ = nullptr;
};

class PrinterSettings final {
public:
    PrinterSettings() noexcept = default;
    PrinterSettings(std::wstring printer_name, OwnedGlobalMemory dev_mode,
                    OwnedGlobalMemory dev_names) noexcept;
    PrinterSettings(const PrinterSettings&) = delete;
    PrinterSettings& operator=(const PrinterSettings&) = delete;
    PrinterSettings(PrinterSettings&&) noexcept = default;
    PrinterSettings& operator=(PrinterSettings&&) noexcept = default;

    PrinterSettings Clone() const noexcept;
    bool IsValid() const noexcept;
    const std::wstring& PrinterName() const noexcept { return printer_name_; }
    LockedGlobalMemory LockDevMode() const noexcept { return dev_mode_.Lock(); }
    LockedGlobalMemory LockDevNames() const noexcept { return dev_names_.Lock(); }

private:
    friend PrintDialogResult ShowPrintDialog(HWND owner, PrinterSettings settings,
                                             DWORD flags) noexcept;
    friend PrintBackendResult CreatePrinterBackend(const PrinterSettings& settings);
    friend struct PrinterSettingsResult;
    std::wstring printer_name_;
    OwnedGlobalMemory dev_mode_;
    OwnedGlobalMemory dev_names_;
};

struct PrinterSettingsResult {
    PrinterSettings settings;
    PrintOperationStatus status = PrintOperationStatus::native_failure;
    DWORD error = ERROR_GEN_FAILURE;
    explicit operator bool() const noexcept { return settings.IsValid(); }
};

// Loads the printer's current defaults without showing UI. The result owns both
// DEVMODE and DEVNAMES blocks and may be moved across scopes, not threads in use.
PrinterSettingsResult LoadPrinterSettings(std::wstring_view printer_name);
PrintBackendResult CreatePrinterBackend(const PrinterSettings& settings);

enum class PrintDialogAction { accepted, cancelled, failed };

struct PrintDialogResult {
    PrinterSettings settings;
    PrintDialogAction action = PrintDialogAction::failed;
    DWORD error = ERROR_GEN_FAILURE;
    DWORD flags = 0;
    DWORD copies = 1;
    explicit operator bool() const noexcept { return action == PrintDialogAction::accepted; }
};

// Must be called on a UI thread. Consumes the supplied settings so any handles
// reallocated by PrintDlgExW have one clear owner on every return path.
PrintDialogResult ShowPrintDialog(HWND owner, PrinterSettings settings,
                                  DWORD flags = 0) noexcept;

}  // namespace mwtl
