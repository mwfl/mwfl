#pragma once

#include <windows.h>

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace mwtl {

enum class FileAssociationStatus {
    success,
    not_found,
    conflict,
    access_denied,
    invalid_argument,
    failed,
};

struct FileAssociationVerb {
    std::wstring id = L"open";
    std::wstring display_name;
    // Optional fixed arguments placed before an automatically quoted "%1".
    std::wstring arguments;
};

struct FileAssociationSpec {
    std::wstring extension;
    std::wstring prog_id;
    std::wstring owner_id;
    std::wstring display_name;
    std::filesystem::path executable;
    std::filesystem::path icon;
    std::vector<FileAssociationVerb> verbs{{}};
};

struct FileAssociationRegistryValue {
    std::wstring relative_key;
    std::wstring value_name;
    std::wstring data;
};

struct FileAssociationPlan {
    std::vector<FileAssociationRegistryValue> values;
    std::wstring extension_key;
    std::wstring prog_id_key;
    bool valid = false;
};

struct FileAssociationResult {
    FileAssociationStatus status = FileAssociationStatus::failed;
    LSTATUS error = ERROR_GEN_FAILURE;
    explicit operator bool() const noexcept { return status == FileAssociationStatus::success; }
};

// Pure plan with relative keys. The extension claim is deliberately last so an
// incomplete write cannot redirect files to an incomplete ProgID definition.
FileAssociationPlan BuildFileAssociationPlan(const FileAssociationSpec& spec);

// classes_subkey allows isolated tests. Production callers normally use
// Software\Classes. root is borrowed. Shell notification is optional because
// isolated test roots are not real association stores.
FileAssociationResult RegisterFileAssociation(
    HKEY root, std::wstring_view classes_subkey, const FileAssociationSpec& spec,
    bool notify_shell = true) noexcept;
FileAssociationResult RemoveFileAssociation(
    HKEY root, std::wstring_view classes_subkey, const FileAssociationSpec& spec,
    bool notify_shell = true) noexcept;

inline FileAssociationResult RegisterPerUserFileAssociation(
    const FileAssociationSpec& spec) noexcept {
    return RegisterFileAssociation(HKEY_CURRENT_USER, L"Software\\Classes", spec, true);
}
inline FileAssociationResult RemovePerUserFileAssociation(
    const FileAssociationSpec& spec) noexcept {
    return RemoveFileAssociation(HKEY_CURRENT_USER, L"Software\\Classes", spec, true);
}

}  // namespace mwtl
