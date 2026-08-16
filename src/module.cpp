#include "detail/module.h"

#include <mutex>

WTL::CAppModule _Module;

namespace mwfl::detail {
namespace {

std::mutex module_mutex;
int module_holds = 0;
bool module_initialized = false;

// Constructed on the first successful Init, so it is destroyed before the
// global _Module and terminates the module exactly once at process exit.
struct ModuleTerminator {
    ~ModuleTerminator() noexcept { _Module.Term(); }
};

}  // namespace

HRESULT AcquireModule(HINSTANCE instance) noexcept {
    const std::lock_guard<std::mutex> lock(module_mutex);
    if (!module_initialized) {
        const HRESULT result = _Module.Init(nullptr, instance);
        if (FAILED(result)) return result;
        static ModuleTerminator terminator;
        module_initialized = true;
    }
    ++module_holds;
    return S_OK;
}

void ReleaseModule() noexcept {
    const std::lock_guard<std::mutex> lock(module_mutex);
    if (module_holds > 0) --module_holds;
}

}  // namespace mwfl::detail
