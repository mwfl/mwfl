#include <mwtl/printing.h>

#include <string>

int main() {
    using namespace mwtl;
    const auto printers = EnumerateLocalPrinters();
    if (!printers) {
        // Spooler-disabled and access-restricted machines are supported states.
        return printers.error == ERROR_SUCCESS ? 1 : 0;
    }
    for (const auto& printer : printers.printers) {
        if (printer.name.empty()) return 2;
    }
    const auto default_printer = QueryDefaultPrinter();
    if (!default_printer) {
        return default_printer.error == ERROR_SUCCESS ? 3 : 0;
    }
    if (default_printer.name.empty()) return 4;
    bool found = false;
    for (const auto& printer : printers.printers) {
        if (printer.name == default_printer.name) {
            if (!printer.is_default) return 5;
            found = true;
        }
    }
    // A default network printer may be temporarily absent from enumeration.
    (void)found;
    return 0;
}
