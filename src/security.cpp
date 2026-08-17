#include <mwfl/security.h>
#include <wincrypt.h>
#include <cstring>
namespace mwfl { namespace {
DATA_BLOB Blob(std::span<const std::byte> bytes) { return {static_cast<DWORD>(bytes.size()), reinterpret_cast<BYTE*>(const_cast<std::byte*>(bytes.data()))}; }
DATA_BLOB Entropy(std::wstring_view purpose) { return {static_cast<DWORD>(purpose.size() * sizeof(wchar_t)), reinterpret_cast<BYTE*>(const_cast<wchar_t*>(purpose.data()))}; }
}  // namespace
SecureBytes::~SecureBytes() { Clear(); }
SecureBytes::SecureBytes(SecureBytes&& other) noexcept : bytes_(std::move(other.bytes_)) { other.bytes_.clear(); }
SecureBytes& SecureBytes::operator=(SecureBytes&& other) noexcept { if (this != &other) { Clear(); bytes_ = std::move(other.bytes_); other.bytes_.clear(); } return *this; }
void SecureBytes::Clear() noexcept { if (!bytes_.empty()) SecureZeroMemory(bytes_.data(), bytes_.size()); bytes_.clear(); bytes_.shrink_to_fit(); }
Result<ProtectedData> ProtectForCurrentUser(std::span<const std::byte> plaintext, std::wstring_view purpose) {
    if (plaintext.size() > MAXDWORD || purpose.size() > MAXDWORD / sizeof(wchar_t)) return NativeError::FromWin32(ERROR_INVALID_PARAMETER);
    DATA_BLOB input = Blob(plaintext), output{}, entropy = Entropy(purpose);
    if (!CryptProtectData(&input, L"mwfl protected data", purpose.empty() ? nullptr : &entropy, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &output)) return NativeError::LastWin32();
    ProtectedData result(output.cbData); std::memcpy(result.data(), output.pbData, output.cbData); LocalFree(output.pbData); return result;
}
Result<SecureBytes> UnprotectForCurrentUser(std::span<const std::byte> protected_data, std::wstring_view purpose) {
    if (protected_data.empty() || protected_data.size() > MAXDWORD || purpose.size() > MAXDWORD / sizeof(wchar_t)) return NativeError::FromWin32(ERROR_INVALID_PARAMETER);
    DATA_BLOB input = Blob(protected_data), output{}, entropy = Entropy(purpose);
    if (!CryptUnprotectData(&input, nullptr, purpose.empty() ? nullptr : &entropy, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &output)) return NativeError::LastWin32();
    std::vector<std::byte> bytes(output.cbData); std::memcpy(bytes.data(), output.pbData, output.cbData); SecureZeroMemory(output.pbData, output.cbData); LocalFree(output.pbData); return SecureBytes(std::move(bytes));
}
}  // namespace mwfl
