#pragma once
#include <cstddef>
#include <mwfl/core.h>
#include <span>
#include <string>
#include <string_view>
#include <vector>
namespace mwfl {
class SecureBytes final {
   public:
    SecureBytes() = default;
    explicit SecureBytes(std::span<const std::byte> bytes);
    ~SecureBytes();
    SecureBytes(SecureBytes&& other) noexcept;
    SecureBytes& operator=(SecureBytes&& other) noexcept;
    SecureBytes(const SecureBytes&) = delete;
    SecureBytes& operator=(const SecureBytes&) = delete;
    [[nodiscard]] std::span<const std::byte> View() const noexcept { return {data_, size_}; }
    [[nodiscard]] std::span<std::byte> MutableView() noexcept { return {data_, size_}; }
    [[nodiscard]] std::size_t Size() const noexcept { return size_; }
    void Clear() noexcept;

   private:
    std::byte* data_ = nullptr;
    std::size_t size_ = 0;
};
class SecureWideString final {
   public:
    SecureWideString() = default;
    explicit SecureWideString(std::wstring_view text);
    ~SecureWideString();
    SecureWideString(SecureWideString&& other) noexcept;
    SecureWideString& operator=(SecureWideString&& other) noexcept;
    SecureWideString(const SecureWideString&) = delete;
    SecureWideString& operator=(const SecureWideString&) = delete;
    [[nodiscard]] std::wstring_view View() const noexcept { return {data_, size_}; }
    void Clear() noexcept;

   private:
    wchar_t* data_ = nullptr;
    std::size_t size_ = 0;
};
using ProtectedData = std::vector<std::byte>;
enum class DataProtectionScope { CurrentUser, LocalMachine };
[[nodiscard]] Result<ProtectedData> ProtectData(
    std::span<const std::byte> plaintext,
    DataProtectionScope scope = DataProtectionScope::CurrentUser, std::wstring_view purpose = {});
[[nodiscard]] Result<SecureBytes> UnprotectData(std::span<const std::byte> protected_data,
                                                std::wstring_view purpose = {});
[[nodiscard]] Result<ProtectedData> ProtectForCurrentUser(std::span<const std::byte> plaintext,
                                                          std::wstring_view purpose = {});
[[nodiscard]] Result<SecureBytes> UnprotectForCurrentUser(std::span<const std::byte> protected_data,
                                                          std::wstring_view purpose = {});
enum class CredentialPersistence { Session, LocalMachine, Enterprise };
struct GenericCredential {
    std::wstring target;
    std::wstring user_name;
    SecureBytes secret;
    CredentialPersistence persistence = CredentialPersistence::Session;
};
class CredentialManager final {
   public:
    [[nodiscard]] static Result<void> Write(GenericCredential credential);
    [[nodiscard]] static Result<GenericCredential> Read(std::wstring_view target);
    [[nodiscard]] static Result<bool> Remove(std::wstring_view target);
    [[nodiscard]] static Result<std::vector<std::wstring>> Enumerate(std::wstring_view filter = {});
};
struct TokenIdentity {
    std::wstring user_sid;
    DWORD session_id = 0;
    bool elevated = false;
    bool administrator = false;
    DWORD integrity_level = 0;
};
[[nodiscard]] Result<TokenIdentity> QueryCurrentProcessIdentity();
[[nodiscard]] Result<TokenIdentity> QueryCurrentThreadIdentity(bool open_as_self = true);
enum class AccessRuleKind { Allow, Deny };
struct SecurityAccessRule {
    AccessRuleKind kind = AccessRuleKind::Allow;
    std::wstring sid;
    DWORD access_mask = GENERIC_ALL;
    BYTE inheritance = 0;
};
class SecurityDescriptorBuilder final {
   public:
    SecurityDescriptorBuilder& Owner(std::wstring sid);
    SecurityDescriptorBuilder& Rule(SecurityAccessRule rule);
    [[nodiscard]] Result<std::vector<std::byte>> Build() const;

   private:
    std::wstring owner_;
    std::vector<SecurityAccessRule> rules_;
};
}  // namespace mwfl
