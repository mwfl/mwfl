#pragma once

#include <windows.h>
#include <aclapi.h>

#include <cstddef>
#include <string>
#include <vector>

class DeniedRegistryRoot final {
public:
    explicit DeniedRegistryRoot(std::wstring path) : path_(std::move(path)) {
        if (::RegCreateKeyExW(HKEY_CURRENT_USER, path_.c_str(), 0, nullptr, 0,
                              KEY_ALL_ACCESS, nullptr, &key_, nullptr) != ERROR_SUCCESS)
            return;
        HANDLE token = nullptr;
        if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &token)) return;
        DWORD bytes = 0;
        ::GetTokenInformation(token, TokenUser, nullptr, 0, &bytes);
        std::vector<std::byte> buffer(bytes);
        if (!bytes || !::GetTokenInformation(token, TokenUser, buffer.data(), bytes, &bytes)) {
            ::CloseHandle(token);
            return;
        }
        ::CloseHandle(token);
        const auto* user = reinterpret_cast<const TOKEN_USER*>(buffer.data());
        EXPLICIT_ACCESSW access{};
        access.grfAccessPermissions = KEY_CREATE_SUB_KEY | KEY_SET_VALUE | DELETE;
        access.grfAccessMode = DENY_ACCESS;
        access.grfInheritance = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
        access.Trustee.TrusteeForm = TRUSTEE_IS_SID;
        access.Trustee.TrusteeType = TRUSTEE_IS_USER;
        access.Trustee.ptstrName = static_cast<LPWSTR>(user->User.Sid);
        PACL acl = nullptr;
        if (::SetEntriesInAclW(1, &access, nullptr, &acl) != ERROR_SUCCESS) return;
        const DWORD result = ::SetSecurityInfo(
            key_, SE_REGISTRY_KEY,
            DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
            nullptr, nullptr, acl, nullptr);
        ::LocalFree(acl);
        denied_ = result == ERROR_SUCCESS;
    }

    ~DeniedRegistryRoot() {
        if (key_) {
            ::SetSecurityInfo(key_, SE_REGISTRY_KEY,
                              DACL_SECURITY_INFORMATION | UNPROTECTED_DACL_SECURITY_INFORMATION,
                              nullptr, nullptr, nullptr, nullptr);
            ::RegCloseKey(key_);
        }
        ::RegDeleteTreeW(HKEY_CURRENT_USER, path_.c_str());
    }
    DeniedRegistryRoot(const DeniedRegistryRoot&) = delete;
    DeniedRegistryRoot& operator=(const DeniedRegistryRoot&) = delete;
    bool IsValid() const noexcept { return key_ && denied_; }
    HKEY Get() const noexcept { return key_; }

private:
    std::wstring path_;
    HKEY key_ = nullptr;
    bool denied_ = false;
};
