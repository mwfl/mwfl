#pragma once

#include <windows.h>

// ATL requires this include order.
// clang-format off
#include <atlbase.h>
#include <atlapp.h>
// clang-format on

extern WTL::CAppModule _Module;

namespace mwfl::detail {

// Process-wide hold on the WTL module behind every Application run.
//
// ATL's CAtlModule::Term destroys the module critical sections and cannot be
// re-initialized by a later Init, so terminating the module between runs made
// the second Application in a process crash (Debug at window creation, Release
// inside Term itself). The module is therefore initialized on the first
// acquisition, kept alive across runs by a reference count, and terminated
// exactly once at process exit. Acquire returns the Init HRESULT (S_OK when the
// module is already initialized); each successful Acquire pairs with one
// Release. Both are thread-safe.
HRESULT AcquireModule(HINSTANCE instance) noexcept;
void ReleaseModule() noexcept;

}  // namespace mwfl::detail
