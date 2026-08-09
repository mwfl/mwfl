#include <mwtl/settings_store.h>

#include <array>
#include <string>

namespace {
struct TestKey {
    std::wstring path = L"Software\\mwtl\\Tests\\SettingsStore-" +
                        std::to_wstring(::GetCurrentProcessId());
    ~TestKey() { ::RegDeleteTreeW(HKEY_CURRENT_USER, path.c_str()); }
};
}

int main() {
    using namespace mwtl;
    TestKey key;
    VersionedSettingsStore store(HKEY_CURRENT_USER, key.path, 3);
    const std::array schema{
        SettingDefinition{L"Enabled", SettingType::dword, sizeof(DWORD), true},
        SettingDefinition{L"Name", SettingType::string, 256, true},
        SettingDefinition{L"Counter", SettingType::qword, sizeof(std::uint64_t), false},
        SettingDefinition{L"Payload", SettingType::binary, 64, false}};
    if (store.Load(schema).status != SettingsStatus::not_found) return 1;
    const std::array values{
        SettingValue{L"Enabled", std::uint32_t{1}},
        SettingValue{L"Name", std::wstring{L"Agent 中文"}},
        SettingValue{L"Counter", std::uint64_t{42}},
        SettingValue{L"Payload", std::vector<std::byte>{std::byte{1}, std::byte{2}}}};
    const auto saved = store.Save(values);
    if (!saved) return saved.status == SettingsStatus::access_denied ? 0 : 2;
    const auto loaded = store.Load(schema);
    if (!loaded || loaded.values.size() != 4) return 3;
    const auto* name = FindSetting(loaded.values, L"Name");
    if (!name || std::get<std::wstring>(name->data) != L"Agent 中文") return 4;

    HKEY raw = nullptr;
    if (::RegOpenKeyExW(HKEY_CURRENT_USER, key.path.c_str(), 0, KEY_SET_VALUE, &raw) != ERROR_SUCCESS)
        return 5;
    const DWORD unrelated = 9;
    ::RegSetValueExW(raw, L"Unrelated", 0, REG_DWORD,
                     reinterpret_cast<const BYTE*>(&unrelated), sizeof(unrelated));
    const DWORD wrong_version = 99;
    ::RegSetValueExW(raw, L"SchemaVersion", 0, REG_DWORD,
                     reinterpret_cast<const BYTE*>(&wrong_version), sizeof(wrong_version));
    ::RegCloseKey(raw);
    if (store.Load(schema).status != SettingsStatus::version_mismatch) return 6;
    if (!store.Save(values)) return 7;
    if (::RegOpenKeyExW(HKEY_CURRENT_USER, key.path.c_str(), 0, KEY_SET_VALUE, &raw) != ERROR_SUCCESS)
        return 12;
    const wchar_t wrong_type[] = L"not a dword";
    ::RegSetValueExW(raw, L"Enabled", 0, REG_SZ,
                     reinterpret_cast<const BYTE*>(wrong_type), sizeof(wrong_type));
    ::RegCloseKey(raw);
    if (store.Load(schema).status != SettingsStatus::malformed || !store.Save(values)) return 13;
    const std::array<std::wstring_view, 4> owned{L"Enabled", L"Name", L"Counter", L"Payload"};
    if (!store.RemoveOwned(owned)) return 8;
    DWORD preserved = 0, bytes = sizeof(preserved);
    if (::RegGetValueW(HKEY_CURRENT_USER, key.path.c_str(), L"Unrelated", RRF_RT_REG_DWORD,
                       nullptr, &preserved, &bytes) != ERROR_SUCCESS || preserved != 9)
        return 9;
    if (store.Load(schema).status != SettingsStatus::malformed) return 10;

    const std::array duplicate{SettingValue{L"A", std::uint32_t{1}},
                               SettingValue{L"A", std::uint32_t{2}}};
    if (store.Save(duplicate).status != SettingsStatus::invalid_argument) return 11;
    const std::array<std::wstring_view, 2> invalid_owned{L"Enabled", L"SchemaVersion"};
    if (store.RemoveOwned(invalid_owned).status != SettingsStatus::invalid_argument) return 14;
    return 0;
}
