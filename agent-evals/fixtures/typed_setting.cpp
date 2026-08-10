#include <mwfl/settings_store.h>

#include <array>

mwfl::VersionedSettingsWriteResult SaveFixtureSetting(bool enabled) {
    mwfl::VersionedSettingsStore store{
        HKEY_CURRENT_USER, L"Software\\mwfl\\AgentFixtures\\TypedSetting", 1};
    const std::array values{
        mwfl::SettingValue{L"Enabled", std::uint32_t{enabled ? 1U : 0U}}};
    return store.Save(values);
}
