#include <mwfl/deployment.h>
#include <appmodel.h>
#include <limits>
namespace mwfl {
Result<PackageIdentity> QueryCurrentPackageIdentity() {
    UINT32 length = 0; LONG result = GetCurrentPackageFullName(&length, nullptr);
    if (result == APPMODEL_ERROR_NO_PACKAGE) return PackageIdentity{};
    if (result != ERROR_INSUFFICIENT_BUFFER) return NativeError::FromWin32(static_cast<DWORD>(result));
    std::wstring name(length, L'\0'); result = GetCurrentPackageFullName(&length, name.data());
    if (result != ERROR_SUCCESS) return NativeError::FromWin32(static_cast<DWORD>(result));
    if (!name.empty() && name.back() == L'\0') name.pop_back();
    return PackageIdentity{true, std::move(name)};
}
Result<void> RegisterApplicationRestart(std::wstring_view arguments, DWORD flags) {
    if (arguments.size() >= RESTART_MAX_CMD_LINE) return NativeError::FromWin32(ERROR_INVALID_PARAMETER);
    std::wstring copy(arguments); const HRESULT result = ::RegisterApplicationRestart(copy.c_str(), flags);
    if (FAILED(result)) return NativeError::FromHResult(result); return {};
}
Result<void> UnregisterApplicationRestart() noexcept { const HRESULT result = ::UnregisterApplicationRestart(); if (FAILED(result)) return NativeError::FromHResult(result); return {}; }
RestartRegistration::RestartRegistration(std::wstring_view arguments, DWORD flags) { active_ = static_cast<bool>(RegisterApplicationRestart(arguments, flags)); }
RestartRegistration::~RestartRegistration() { if (active_) (void)UnregisterApplicationRestart(); }
RestartRegistration::RestartRegistration(RestartRegistration&& other) noexcept : active_(std::exchange(other.active_, false)) {}
RestartRegistration& RestartRegistration::operator=(RestartRegistration&& other) noexcept { if (this != &other) { if (active_) (void)UnregisterApplicationRestart(); active_ = std::exchange(other.active_, false); } return *this; }
}  // namespace mwfl
