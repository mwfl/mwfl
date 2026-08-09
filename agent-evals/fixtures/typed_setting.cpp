#include <mwtl/settings_store.h>

#include <array>

mwtl::VersionedSettingsWriteResult SaveFixtureSetting(bool enabled) {
    mwtl::VersionedSettingsStore store{
        HKEY_CURRENT_USER, L"Software\\mwtl\\AgentFixtures\\TypedSetting", 1};
    const std::array values{
        mwtl::SettingValue{L"Enabled", std::uint32_t{enabled ? 1U : 0U}}};
    return store.Save(values);
}
