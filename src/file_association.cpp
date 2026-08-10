#include <mwtl/file_association.h>

#include <shlobj_core.h>
#include <wil/resource.h>

#include <algorithm>
#include <cwctype>
#include <limits>
#include <utility>

namespace mwtl {
namespace {

constexpr wchar_t kOwnerValue[] = L"mwtl.OwnerId";

bool Clean(std::wstring_view value) noexcept {
    return !value.empty() && value.find(L'\0') == std::wstring_view::npos;
}

bool ValidComponent(std::wstring_view value) noexcept {
    return Clean(value) && value.find(L'\\') == std::wstring_view::npos &&
           value.find(L'/') == std::wstring_view::npos;
}

bool ValidVerb(std::wstring_view value) noexcept {
    return ValidComponent(value) && std::all_of(value.begin(), value.end(), [](wchar_t ch) {
        return std::iswalnum(ch) || ch == L'_' || ch == L'-';
    });
}

std::wstring Join(std::wstring_view left, std::wstring_view right) {
    std::wstring result(left);
    if (!result.empty() && result.back() != L'\\') result.push_back(L'\\');
    result.append(right);
    return result;
}

std::wstring Quote(const std::filesystem::path& value) {
    return L"\"" + value.native() + L"\"";
}

FileAssociationStatus MapError(LSTATUS error) noexcept {
    if (error == ERROR_SUCCESS) return FileAssociationStatus::success;
    if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND)
        return FileAssociationStatus::not_found;
    if (error == ERROR_ACCESS_DENIED) return FileAssociationStatus::access_denied;
    if (error == ERROR_INVALID_PARAMETER) return FileAssociationStatus::invalid_argument;
    return FileAssociationStatus::failed;
}

struct StringQuery {
    std::wstring value;
    LSTATUS error = ERROR_GEN_FAILURE;
    bool found = false;
};

StringQuery ReadString(HKEY root, std::wstring_view key_path,
                       const wchar_t* value_name) {
    StringQuery result;
    const std::wstring key(key_path);
    DWORD bytes = 0;
    result.error = ::RegGetValueW(root, key.c_str(), value_name, RRF_RT_REG_SZ,
                                  nullptr, nullptr, &bytes);
    if (result.error != ERROR_SUCCESS) return result;
    if (bytes < sizeof(wchar_t) || bytes % sizeof(wchar_t) != 0) {
        result.error = ERROR_INVALID_DATA;
        return result;
    }
    std::wstring buffer(bytes / sizeof(wchar_t), L'\0');
    result.error = ::RegGetValueW(root, key.c_str(), value_name, RRF_RT_REG_SZ,
                                  nullptr, buffer.data(), &bytes);
    if (result.error != ERROR_SUCCESS || buffer.empty()) return result;
    const auto terminator = std::find(buffer.begin(), buffer.end(), L'\0');
    if (terminator == buffer.end() ||
        std::any_of(terminator, buffer.end(), [](wchar_t value) { return value != L'\0'; })) {
        result.error = ERROR_INVALID_DATA;
        return result;
    }
    result.value.assign(buffer.begin(), terminator);
    result.found = true;
    return result;
}

LSTATUS WriteString(HKEY root, std::wstring_view key_path,
                    std::wstring_view value_name, std::wstring_view data) {
    if (data.size() > ((std::numeric_limits<DWORD>::max)() / sizeof(wchar_t)) - 1)
        return ERROR_INVALID_PARAMETER;
    wil::unique_hkey key;
    const std::wstring path(key_path);
    LSTATUS error = ::RegCreateKeyExW(root, path.c_str(), 0, nullptr, 0, KEY_SET_VALUE,
                                      nullptr, key.put(), nullptr);
    if (error != ERROR_SUCCESS) return error;
    const std::wstring name(value_name);
    return ::RegSetValueExW(key.get(), name.c_str(), 0, REG_SZ,
                            reinterpret_cast<const BYTE*>(data.data()),
                            static_cast<DWORD>((data.size() + 1) * sizeof(wchar_t)));
}

bool KeyExists(HKEY root, std::wstring_view key_path) noexcept {
    wil::unique_hkey key;
    const std::wstring path(key_path);
    return ::RegOpenKeyExW(root, path.c_str(), 0, KEY_QUERY_VALUE, key.put()) == ERROR_SUCCESS;
}

void DeleteKeyIfEmpty(HKEY root, std::wstring_view key_path) noexcept {
    wil::unique_hkey key;
    const std::wstring path(key_path);
    if (::RegOpenKeyExW(root, path.c_str(), 0, KEY_QUERY_VALUE, key.put()) != ERROR_SUCCESS) return;
    DWORD subkeys = 0, values = 0;
    if (::RegQueryInfoKeyW(key.get(), nullptr, nullptr, nullptr, &subkeys, nullptr, nullptr,
                           &values, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS)
        return;
    key.reset();
    if (subkeys == 0 && values == 0) ::RegDeleteKeyW(root, path.c_str());
}

void NotifyShell(bool enabled) noexcept {
    if (enabled) ::SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
}

}  // namespace

FileAssociationPlan BuildFileAssociationPlan(const FileAssociationSpec& spec) {
    FileAssociationPlan plan;
    if (spec.extension.size() < 2 || spec.extension.front() != L'.' ||
        !ValidComponent(spec.extension) || !ValidComponent(spec.prog_id) ||
        !ValidComponent(spec.owner_id) || !Clean(spec.display_name) ||
        spec.executable.empty() || !spec.executable.is_absolute() ||
        (!spec.icon.empty() && !spec.icon.is_absolute()) || spec.verbs.empty())
        return plan;
    const std::wstring executable = spec.executable.native();
    const std::wstring icon = spec.icon.native();
    if (executable.find(L'"') != std::wstring::npos ||
        icon.find(L'"') != std::wstring::npos) return plan;
    for (const auto& verb : spec.verbs) {
        if (!ValidVerb(verb.id) || verb.arguments.find(L'\0') != std::wstring::npos ||
            verb.arguments.find(L"%1") != std::wstring::npos)
            return plan;
    }
    plan.extension_key = spec.extension;
    plan.prog_id_key = spec.prog_id;
    plan.values.push_back({spec.prog_id, L"", spec.display_name});
    plan.values.push_back({spec.prog_id, kOwnerValue, spec.owner_id});
    if (!spec.icon.empty())
        plan.values.push_back({Join(spec.prog_id, L"DefaultIcon"), L"", Quote(spec.icon)});
    for (const auto& verb : spec.verbs) {
        const std::wstring verb_key = Join(Join(spec.prog_id, L"shell"), verb.id);
        if (!verb.display_name.empty()) plan.values.push_back({verb_key, L"", verb.display_name});
        std::wstring command = Quote(spec.executable);
        if (!verb.arguments.empty()) command += L" " + verb.arguments;
        command += L" \"%1\"";
        plan.values.push_back({Join(verb_key, L"command"), L"", std::move(command)});
    }
    // Commit the extension claim last.
    plan.values.push_back({spec.extension, kOwnerValue, spec.owner_id});
    plan.values.push_back({spec.extension, L"", spec.prog_id});
    plan.valid = true;
    return plan;
}

FileAssociationResult RegisterFileAssociation(
    HKEY root, std::wstring_view classes_subkey, const FileAssociationSpec& spec,
    bool notify_shell) noexcept {
    try {
        const auto plan = BuildFileAssociationPlan(spec);
        if (!root || !Clean(classes_subkey) || !plan.valid)
            return {FileAssociationStatus::invalid_argument, ERROR_INVALID_PARAMETER};
        const std::wstring extension_key = Join(classes_subkey, plan.extension_key);
        const std::wstring prog_id_key = Join(classes_subkey, plan.prog_id_key);
        const auto existing_prog_owner = ReadString(root, prog_id_key, kOwnerValue);
        if (KeyExists(root, prog_id_key) &&
            (!existing_prog_owner.found || existing_prog_owner.value != spec.owner_id))
            return {FileAssociationStatus::conflict, ERROR_ALREADY_EXISTS};
        const auto existing_extension = ReadString(root, extension_key, nullptr);
        const auto existing_extension_owner = ReadString(root, extension_key, kOwnerValue);
        if (existing_extension.found &&
            (existing_extension.value != spec.prog_id || !existing_extension_owner.found ||
             existing_extension_owner.value != spec.owner_id))
            return {FileAssociationStatus::conflict, ERROR_ALREADY_EXISTS};
        if (KeyExists(root, extension_key) && existing_extension_owner.found &&
            existing_extension_owner.value != spec.owner_id)
            return {FileAssociationStatus::conflict, ERROR_NOT_OWNER};
        for (const auto& value : plan.values) {
            const LSTATUS error = WriteString(root, Join(classes_subkey, value.relative_key),
                                              value.value_name, value.data);
            if (error != ERROR_SUCCESS) return {MapError(error), error};
        }
        NotifyShell(notify_shell);
        return {FileAssociationStatus::success, ERROR_SUCCESS};
    } catch (...) {
        return {FileAssociationStatus::failed, ERROR_OUTOFMEMORY};
    }
}

FileAssociationResult RemoveFileAssociation(
    HKEY root, std::wstring_view classes_subkey, const FileAssociationSpec& spec,
    bool notify_shell) noexcept {
    try {
        const auto plan = BuildFileAssociationPlan(spec);
        if (!root || !Clean(classes_subkey) || !plan.valid)
            return {FileAssociationStatus::invalid_argument, ERROR_INVALID_PARAMETER};
        const std::wstring extension_key = Join(classes_subkey, plan.extension_key);
        const std::wstring prog_id_key = Join(classes_subkey, plan.prog_id_key);
        const auto prog_owner = ReadString(root, prog_id_key, kOwnerValue);
        if (!prog_owner.found) {
            if (KeyExists(root, prog_id_key) || KeyExists(root, extension_key))
                return {FileAssociationStatus::conflict, ERROR_NOT_OWNER};
            return {FileAssociationStatus::not_found, prog_owner.error};
        }
        if (prog_owner.value != spec.owner_id)
            return {FileAssociationStatus::conflict, ERROR_NOT_OWNER};
        const auto extension_owner = ReadString(root, extension_key, kOwnerValue);
        if (!extension_owner.found)
            return {FileAssociationStatus::conflict, ERROR_NOT_OWNER};
        if (extension_owner.value != spec.owner_id)
            return {FileAssociationStatus::conflict, ERROR_INVALID_OWNER};
        const auto extension_value = ReadString(root, extension_key, nullptr);
        if (!extension_value.found || extension_value.value != spec.prog_id)
            return {FileAssociationStatus::conflict, ERROR_NOT_OWNER};

        // The owner marker proves this application owns the entire ProgID subtree.
        LSTATUS error = ::RegDeleteTreeW(root, prog_id_key.c_str());
        if (error != ERROR_SUCCESS && error != ERROR_FILE_NOT_FOUND)
            return {MapError(error), error};
        wil::unique_hkey extension;
        error = ::RegOpenKeyExW(root, extension_key.c_str(), 0,
                                KEY_QUERY_VALUE | KEY_SET_VALUE, extension.put());
        if (error == ERROR_SUCCESS) {
            const LSTATUS first = ::RegDeleteValueW(extension.get(), nullptr);
            const LSTATUS second = ::RegDeleteValueW(extension.get(), kOwnerValue);
            if ((first != ERROR_SUCCESS && first != ERROR_FILE_NOT_FOUND) ||
                (second != ERROR_SUCCESS && second != ERROR_FILE_NOT_FOUND))
                return {MapError(first != ERROR_SUCCESS ? first : second),
                        first != ERROR_SUCCESS ? first : second};
        } else if (error != ERROR_FILE_NOT_FOUND) {
            return {MapError(error), error};
        }
        extension.reset();
        DeleteKeyIfEmpty(root, extension_key);
        NotifyShell(notify_shell);
        return {FileAssociationStatus::success, ERROR_SUCCESS};
    } catch (...) {
        return {FileAssociationStatus::failed, ERROR_OUTOFMEMORY};
    }
}

}  // namespace mwtl
