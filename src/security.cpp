#include <algorithm>
#include <cstring>
#include <mwfl/security.h>
#include <sddl.h>
#include <wincred.h>
#include <wincrypt.h>
namespace mwfl {
namespace {
DATA_BLOB Blob(std::span<const std::byte> bytes) {
    return {static_cast<DWORD>(bytes.size()),
            reinterpret_cast<BYTE*>(const_cast<std::byte*>(bytes.data()))};
}
DATA_BLOB Entropy(std::wstring_view purpose) {
    return {static_cast<DWORD>(purpose.size() * sizeof(wchar_t)),
            reinterpret_cast<BYTE*>(const_cast<wchar_t*>(purpose.data()))};
}
DWORD Persist(CredentialPersistence value) {
    switch (value) {
        case CredentialPersistence::LocalMachine:
            return CRED_PERSIST_LOCAL_MACHINE;
        case CredentialPersistence::Enterprise:
            return CRED_PERSIST_ENTERPRISE;
        default:
            return CRED_PERSIST_SESSION;
    }
}
CredentialPersistence Persist(DWORD value) {
    if (value == CRED_PERSIST_LOCAL_MACHINE) return CredentialPersistence::LocalMachine;
    if (value == CRED_PERSIST_ENTERPRISE) return CredentialPersistence::Enterprise;
    return CredentialPersistence::Session;
}
Result<TokenIdentity> IdentityFromToken(HANDLE token) {
    DWORD bytes = 0;
    GetTokenInformation(token, TokenUser, nullptr, 0, &bytes);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) return SystemError::LastWin32();
    std::vector<std::byte> user_storage(bytes);
    if (!GetTokenInformation(token, TokenUser, user_storage.data(), bytes, &bytes))
        return SystemError::LastWin32();
    auto* user = reinterpret_cast<TOKEN_USER*>(user_storage.data());
    wchar_t* sid = nullptr;
    if (!ConvertSidToStringSidW(user->User.Sid, &sid)) return SystemError::LastWin32();
    std::wstring sid_text(sid);
    LocalFree(sid);
    DWORD session = 0;
    bytes = sizeof(session);
    if (!GetTokenInformation(token, TokenSessionId, &session, bytes, &bytes))
        return SystemError::LastWin32();
    TOKEN_ELEVATION elevation{};
    bytes = sizeof(elevation);
    const bool elevated = GetTokenInformation(token, TokenElevation, &elevation, bytes, &bytes) &&
                          elevation.TokenIsElevated != 0;
    BYTE admin_buffer[SECURITY_MAX_SID_SIZE]{};
    DWORD admin_bytes = sizeof(admin_buffer);
    bool admin = false;
    if (CreateWellKnownSid(WinBuiltinAdministratorsSid, nullptr, admin_buffer, &admin_bytes)) {
        BOOL member = FALSE;
        if (CheckTokenMembership(token, admin_buffer, &member)) admin = member != FALSE;
    }
    bytes = 0;
    GetTokenInformation(token, TokenIntegrityLevel, nullptr, 0, &bytes);
    std::vector<std::byte> integrity_storage(bytes);
    DWORD integrity = 0;
    if (bytes &&
        GetTokenInformation(token, TokenIntegrityLevel, integrity_storage.data(), bytes, &bytes)) {
        auto* label = reinterpret_cast<TOKEN_MANDATORY_LABEL*>(integrity_storage.data());
        const DWORD count = *GetSidSubAuthorityCount(label->Label.Sid);
        integrity = *GetSidSubAuthority(label->Label.Sid, count - 1);
    }
    return TokenIdentity{std::move(sid_text), session, elevated, admin, integrity};
}
bool ValidSidText(std::wstring_view value) {
    PSID sid = nullptr;
    std::wstring copy(value);
    const BOOL valid = ConvertStringSidToSidW(copy.c_str(), &sid);
    if (sid) LocalFree(sid);
    return valid != FALSE;
}
}  // namespace
SecureBuffer::SecureBuffer(std::span<const std::byte> bytes) {
    if (!bytes.empty()) {
        data_ = static_cast<std::byte*>(HeapAlloc(GetProcessHeap(), 0, bytes.size()));
        if (!data_) throw std::bad_alloc();
        size_ = bytes.size();
        std::memcpy(data_, bytes.data(), size_);
    }
}
SecureBuffer::~SecureBuffer() {
    Clear();
}
SecureBuffer::SecureBuffer(SecureBuffer&& other) noexcept
    : data_(std::exchange(other.data_, nullptr)), size_(std::exchange(other.size_, 0)) {}
SecureBuffer& SecureBuffer::operator=(SecureBuffer&& other) noexcept {
    if (this != &other) {
        Clear();
        data_ = std::exchange(other.data_, nullptr);
        size_ = std::exchange(other.size_, 0);
    }
    return *this;
}
void SecureBuffer::Clear() noexcept {
    if (data_) {
        SecureZeroMemory(data_, size_);
        HeapFree(GetProcessHeap(), 0, data_);
        data_ = nullptr;
        size_ = 0;
    }
}
SecureString::SecureString(std::wstring_view text) {
    if (!text.empty()) {
        data_ = static_cast<wchar_t*>(
            HeapAlloc(GetProcessHeap(), 0, (text.size() + 1) * sizeof(wchar_t)));
        if (!data_) throw std::bad_alloc();
        size_ = text.size();
        std::memcpy(data_, text.data(), size_ * sizeof(wchar_t));
        data_[size_] = L'\0';
    }
}
SecureString::~SecureString() {
    Clear();
}
SecureString::SecureString(SecureString&& other) noexcept
    : data_(std::exchange(other.data_, nullptr)), size_(std::exchange(other.size_, 0)) {}
SecureString& SecureString::operator=(SecureString&& other) noexcept {
    if (this != &other) {
        Clear();
        data_ = std::exchange(other.data_, nullptr);
        size_ = std::exchange(other.size_, 0);
    }
    return *this;
}
void SecureString::Clear() noexcept {
    if (data_) {
        SecureZeroMemory(data_, (size_ + 1) * sizeof(wchar_t));
        HeapFree(GetProcessHeap(), 0, data_);
        data_ = nullptr;
        size_ = 0;
    }
}
Result<ProtectedData> ProtectData(std::span<const std::byte> plaintext, DataProtectionScope scope,
                                  std::wstring_view purpose) {
    if (plaintext.size() > MAXDWORD || purpose.size() > MAXDWORD / sizeof(wchar_t))
        return SystemError::FromWin32(ERROR_INVALID_PARAMETER);
    DATA_BLOB input = Blob(plaintext), output{}, entropy = Entropy(purpose);
    DWORD flags = CRYPTPROTECT_UI_FORBIDDEN |
                  (scope == DataProtectionScope::LocalMachine ? CRYPTPROTECT_LOCAL_MACHINE : 0);
    if (!CryptProtectData(&input, L"mwfl protected data", purpose.empty() ? nullptr : &entropy,
                          nullptr, nullptr, flags, &output))
        return SystemError::LastWin32();
    ProtectedData result(output.cbData);
    std::memcpy(result.data(), output.pbData, output.cbData);
    SecureZeroMemory(output.pbData, output.cbData);
    LocalFree(output.pbData);
    return result;
}
Result<SecureBuffer> UnprotectData(std::span<const std::byte> protected_data,
                                  std::wstring_view purpose) {
    if (protected_data.empty() || protected_data.size() > MAXDWORD ||
        purpose.size() > MAXDWORD / sizeof(wchar_t))
        return SystemError::FromWin32(ERROR_INVALID_PARAMETER);
    DATA_BLOB input = Blob(protected_data), output{}, entropy = Entropy(purpose);
    if (!CryptUnprotectData(&input, nullptr, purpose.empty() ? nullptr : &entropy, nullptr, nullptr,
                            CRYPTPROTECT_UI_FORBIDDEN, &output))
        return SystemError::LastWin32();
    SecureBuffer bytes(std::span<const std::byte>(reinterpret_cast<const std::byte*>(output.pbData),
                                                 output.cbData));
    SecureZeroMemory(output.pbData, output.cbData);
    LocalFree(output.pbData);
    return bytes;
}
Result<ProtectedData> ProtectForCurrentUser(std::span<const std::byte> plaintext,
                                            std::wstring_view purpose) {
    return ProtectData(plaintext, DataProtectionScope::CurrentUser, purpose);
}
Result<SecureBuffer> UnprotectForCurrentUser(std::span<const std::byte> data,
                                            std::wstring_view purpose) {
    return UnprotectData(data, purpose);
}
Result<void> CredentialManager::Write(GenericCredential credential) {
    if (credential.target.empty() || credential.secret.Size() > CRED_MAX_CREDENTIAL_BLOB_SIZE)
        return SystemError::FromWin32(ERROR_INVALID_PARAMETER);
    CREDENTIALW value{};
    value.Type = CRED_TYPE_GENERIC;
    value.TargetName = credential.target.data();
    value.UserName = credential.user_name.empty() ? nullptr : credential.user_name.data();
    value.CredentialBlobSize = static_cast<DWORD>(credential.secret.Size());
    value.CredentialBlob = reinterpret_cast<BYTE*>(credential.secret.MutableView().data());
    value.Persist = Persist(credential.persistence);
    if (!CredWriteW(&value, 0)) return SystemError::LastWin32().WithOperation(L"CredWriteW");
    return {};
}
Result<GenericCredential> CredentialManager::Read(std::wstring_view target) {
    std::wstring copy(target);
    PCREDENTIALW raw = nullptr;
    if (!CredReadW(copy.c_str(), CRED_TYPE_GENERIC, 0, &raw))
        return SystemError::LastWin32().WithOperation(L"CredReadW");
    GenericCredential result;
    result.target = raw->TargetName ? raw->TargetName : L"";
    result.user_name = raw->UserName ? raw->UserName : L"";
    result.secret = SecureBuffer(std::span<const std::byte>(
        reinterpret_cast<const std::byte*>(raw->CredentialBlob), raw->CredentialBlobSize));
    result.persistence = Persist(raw->Persist);
    CredFree(raw);
    return result;
}
Result<bool> CredentialManager::Remove(std::wstring_view target) {
    std::wstring copy(target);
    if (CredDeleteW(copy.c_str(), CRED_TYPE_GENERIC, 0)) return true;
    if (GetLastError() == ERROR_NOT_FOUND) return false;
    return SystemError::LastWin32().WithOperation(L"CredDeleteW");
}
Result<std::vector<std::wstring>> CredentialManager::Enumerate(std::wstring_view filter) {
    DWORD count = 0;
    PCREDENTIALW* values = nullptr;
    std::wstring copy(filter);
    if (!CredEnumerateW(copy.empty() ? nullptr : copy.c_str(),
                        copy.empty() ? CRED_ENUMERATE_ALL_CREDENTIALS : 0, &count, &values)) {
        if (GetLastError() == ERROR_NOT_FOUND) return std::vector<std::wstring>{};
        return SystemError::LastWin32();
    }
    std::vector<std::wstring> result;
    for (DWORD i = 0; i < count; ++i)
        if (values[i]->Type == CRED_TYPE_GENERIC && values[i]->TargetName)
            result.emplace_back(values[i]->TargetName);
    CredFree(values);
    return result;
}
Result<TokenIdentity> QueryCurrentProcessIdentity() {
    HANDLE raw = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &raw)) return SystemError::LastWin32();
    KernelHandle token(raw);
    return IdentityFromToken(token.Get());
}
Result<TokenIdentity> QueryCurrentThreadIdentity(bool open_as_self) {
    HANDLE raw = nullptr;
    if (!OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, open_as_self, &raw))
        return SystemError::LastWin32();
    KernelHandle token(raw);
    return IdentityFromToken(token.Get());
}
SecurityDescriptorBuilder& SecurityDescriptorBuilder::Owner(std::wstring sid) {
    owner_ = std::move(sid);
    return *this;
}
SecurityDescriptorBuilder& SecurityDescriptorBuilder::Rule(SecurityAccessRule rule) {
    rules_.push_back(std::move(rule));
    return *this;
}
Result<std::vector<std::byte>> SecurityDescriptorBuilder::Build() const {
    if ((!owner_.empty() && !ValidSidText(owner_)) ||
        std::any_of(rules_.begin(), rules_.end(),
                    [](const auto& rule) { return !ValidSidText(rule.sid); }))
        return SystemError::FromWin32(ERROR_INVALID_SID);
    constexpr BYTE supported_inheritance = OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE |
                                           NO_PROPAGATE_INHERIT_ACE | INHERIT_ONLY_ACE |
                                           INHERITED_ACE;
    std::wstring sddl;
    if (!owner_.empty()) sddl += L"O:" + owner_;
    sddl += L"D:P";
    for (const auto& rule : rules_) {
        if ((rule.inheritance & ~supported_inheritance) != 0)
            return SystemError::FromWin32(ERROR_INVALID_FLAGS)
                .WithOperation(L"Security descriptor inheritance");
        wchar_t mask[16]{};
        swprintf_s(mask, L"0x%08X", rule.access_mask);
        sddl += rule.kind == AccessRuleKind::Allow ? L"(A;" : L"(D;";
        if (rule.inheritance & OBJECT_INHERIT_ACE) sddl += L"OI";
        if (rule.inheritance & CONTAINER_INHERIT_ACE) sddl += L"CI";
        if (rule.inheritance & NO_PROPAGATE_INHERIT_ACE) sddl += L"NP";
        if (rule.inheritance & INHERIT_ONLY_ACE) sddl += L"IO";
        if (rule.inheritance & INHERITED_ACE) sddl += L"ID";
        sddl += L";";
        sddl += mask;
        sddl += L";;;" + rule.sid + L")";
    }
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    ULONG bytes = 0;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(), SDDL_REVISION_1,
                                                              &descriptor, &bytes))
        return SystemError::LastWin32().WithOperation(L"Convert security descriptor");
    std::vector<std::byte> result(bytes);
    std::memcpy(result.data(), descriptor, bytes);
    LocalFree(descriptor);
    return result;
}
}  // namespace mwfl
