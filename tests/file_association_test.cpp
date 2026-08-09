#include <mwtl/file_association.h>

#include "denied_registry_root.h"

#include <cstdio>
#include <string>

namespace {
struct TestRoot {
    std::wstring path = L"Software\\mwtl\\Tests\\FileAssociation-" +
                        std::to_wstring(::GetCurrentProcessId());
    ~TestRoot() { ::RegDeleteTreeW(HKEY_CURRENT_USER, path.c_str()); }
};

std::wstring Read(HKEY root, const std::wstring& key, const wchar_t* name = nullptr) {
    wchar_t value[1024]{};
    DWORD bytes = sizeof(value);
    return ::RegGetValueW(root, key.c_str(), name, RRF_RT_REG_SZ, nullptr, value, &bytes) ==
                   ERROR_SUCCESS ? value : L"";
}
}

int main() {
    using namespace mwtl;
    TestRoot root;
    FileAssociationSpec spec{
        .extension = L".mwtltest",
        .prog_id = L"mwtl.Tests.Document",
        .owner_id = L"mwtl-tests-owner",
        .display_name = L"mwtl test document",
        .executable = L"C:\\Program Files\\mwtl test\\app.exe",
        .icon = L"C:\\Program Files\\mwtl test\\app.exe",
        .verbs = {{L"open", L"Open", L""},
                  {L"inspect", L"Inspect", L"--inspect"}}};
    const auto plan = BuildFileAssociationPlan(spec);
    if (!plan.valid || plan.values.size() != 9 ||
        plan.values.back().relative_key != spec.extension ||
        plan.values.back().data != spec.prog_id) return 1;
    if (!RegisterFileAssociation(HKEY_CURRENT_USER, root.path, spec, false)) return 2;
    const std::wstring extension = root.path + L"\\" + spec.extension;
    const std::wstring prog_id = root.path + L"\\" + spec.prog_id;
    if (Read(HKEY_CURRENT_USER, extension) != spec.prog_id ||
        Read(HKEY_CURRENT_USER, extension, L"mwtl.OwnerId") != spec.owner_id ||
        Read(HKEY_CURRENT_USER, prog_id, L"mwtl.OwnerId") != spec.owner_id ||
        Read(HKEY_CURRENT_USER, prog_id + L"\\shell\\open\\command") !=
            L"\"C:\\Program Files\\mwtl test\\app.exe\" \"%1\"") return 3;

    HKEY extension_key = nullptr;
    if (::RegOpenKeyExW(HKEY_CURRENT_USER, extension.c_str(), 0, KEY_SET_VALUE,
                        &extension_key) != ERROR_SUCCESS) return 4;
    const wchar_t unrelated[] = L"preserve";
    ::RegSetValueExW(extension_key, L"Unrelated", 0, REG_SZ,
                     reinterpret_cast<const BYTE*>(unrelated), sizeof(unrelated));
    ::RegCloseKey(extension_key);
    const auto removed = RemoveFileAssociation(HKEY_CURRENT_USER, root.path, spec, false);
    if (!removed) {
        std::fprintf(stderr, "remove failed: status=%d error=%ld\n",
                     static_cast<int>(removed.status), removed.error);
        return 5;
    }
    if (Read(HKEY_CURRENT_USER, extension, L"Unrelated") != L"preserve" ||
        !Read(HKEY_CURRENT_USER, prog_id).empty()) return 6;

    if (::RegCreateKeyExW(HKEY_CURRENT_USER, extension.c_str(), 0, nullptr, 0,
                          KEY_SET_VALUE, nullptr, &extension_key, nullptr) != ERROR_SUCCESS)
        return 15;
    const DWORD spec_bytes = static_cast<DWORD>((spec.prog_id.size() + 1) * sizeof(wchar_t));
    ::RegSetValueExW(extension_key, nullptr, 0, REG_SZ,
                     reinterpret_cast<const BYTE*>(spec.prog_id.c_str()), spec_bytes);
    ::RegCloseKey(extension_key);
    if (RegisterFileAssociation(HKEY_CURRENT_USER, root.path, spec, false).status !=
        FileAssociationStatus::conflict) return 16;

    const wchar_t other[] = L"Other.Application";
    if (::RegCreateKeyExW(HKEY_CURRENT_USER, extension.c_str(), 0, nullptr, 0,
                          KEY_SET_VALUE, nullptr, &extension_key, nullptr) != ERROR_SUCCESS)
        return 7;
    ::RegSetValueExW(extension_key, nullptr, 0, REG_SZ,
                     reinterpret_cast<const BYTE*>(other), sizeof(other));
    ::RegCloseKey(extension_key);
    if (RegisterFileAssociation(HKEY_CURRENT_USER, root.path, spec, false).status !=
        FileAssociationStatus::conflict) return 8;
    if (RemoveFileAssociation(HKEY_CURRENT_USER, root.path, spec, false).status !=
        FileAssociationStatus::conflict) return 9;

    auto invalid = spec;
    invalid.extension = L"txt";
    if (BuildFileAssociationPlan(invalid).valid ||
        RegisterFileAssociation(HKEY_CURRENT_USER, root.path, invalid, false).status !=
            FileAssociationStatus::invalid_argument) return 10;

    DeniedRegistryRoot denied_root{L"Software\\mwtl\\Tests\\DeniedAssociation-" +
                                   std::to_wstring(::GetCurrentProcessId())};
    if (!denied_root.IsValid()) return 17;
    const auto denied = RegisterFileAssociation(denied_root.Get(), L"Classes", spec, false);
    if (denied.status != FileAssociationStatus::access_denied) return 18;
    return 0;
}
