#include <mwtl/settings_store.h>

int main() {
    const mwtl::SettingValue value{L"Name", std::wstring{L"consumer"}};
    return mwtl::GetSettingType(value.data) == mwtl::SettingType::string ? 0 : 1;
}
