#include <mwtl/settings_store.h>
#include <mwtl/file_association.h>
#include <mwtl/shell_integration.h>

int main() {
    const mwtl::SettingValue value{L"Name", std::wstring{L"consumer"}};
    mwtl::FileAssociationSpec association{
        .extension = L".consumer",
        .prog_id = L"mwtl.Consumer.Document",
        .owner_id = L"mwtl-consumer",
        .display_name = L"Consumer document",
        .executable = L"C:\\consumer.exe"};
    mwtl::TaskbarProgressModel progress;
    return mwtl::GetSettingType(value.data) == mwtl::SettingType::string &&
                   mwtl::BuildFileAssociationPlan(association).valid &&
                   progress.SetValue(1, 2) ? 0 : 1;
}
