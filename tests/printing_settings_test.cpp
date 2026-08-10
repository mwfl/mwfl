#include <mwtl/printing_settings.h>

#include <cstddef>

int main() {
    using namespace mwtl;
    auto memory = OwnedGlobalMemory::Allocate(256);
    if (!memory || memory.Size() < 256) return 1;
    {
        auto lock = memory.Lock();
        if (!lock) return 2;
        auto* bytes = lock.As<std::byte>();
        bytes[0] = std::byte{0x2a};
        bytes[255] = std::byte{0x5a};
    }
    auto copy = memory.Clone();
    if (!copy || copy.Get() == memory.Get()) return 3;
    {
        auto lock = copy.Lock();
        const auto* bytes = lock.As<const std::byte>();
        if (!lock || bytes[0] != std::byte{0x2a} || bytes[255] != std::byte{0x5a}) return 4;
    }
    auto moved = std::move(copy);
    if (!moved || copy) return 5;
    PrinterSettings malformed(L"invalid", OwnedGlobalMemory::Allocate(sizeof(DEVMODEW)),
                              OwnedGlobalMemory::Allocate(sizeof(DEVNAMES)));
    if (malformed.IsValid() || CreatePrinterBackend(malformed).status !=
                                   PrintOperationStatus::invalid_settings)
        return 10;

    const auto printers = EnumerateLocalPrinters();
    if (!printers || printers.printers.empty()) return 0;
    auto settings = LoadPrinterSettings(printers.printers.front().name);
    if (!settings) return settings.error == ERROR_ACCESS_DENIED ? 0 : 6;
    auto clone = settings.settings.Clone();
    if (!clone.IsValid() || clone.PrinterName() != settings.settings.PrinterName()) return 7;
    auto mode = clone.LockDevMode();
    auto names = clone.LockDevNames();
    if (!mode || !names || mode.As<const DEVMODEW>()->dmSize < sizeof(DEVMODEW)) return 8;
    if (LoadPrinterSettings(L"").status != PrintOperationStatus::invalid_settings) return 9;
    return 0;
}
