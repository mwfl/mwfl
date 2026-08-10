#include <mwfl/settings_store.h>
#include <mwfl/file_association.h>
#include <mwfl/shell_integration.h>

int main() {
    const mwfl::SettingValue value{L"Name", std::wstring{L"consumer"}};
    mwfl::FileAssociationSpec association{
        .extension = L".consumer",
        .prog_id = L"mwfl.Consumer.Document",
        .owner_id = L"mwfl-consumer",
        .display_name = L"Consumer document",
        .executable = L"C:\\consumer.exe"};
    mwfl::TaskbarProgressModel progress;
    return mwfl::GetSettingType(value.data) == mwfl::SettingType::string &&
                   mwfl::BuildFileAssociationPlan(association).valid &&
                   progress.SetValue(1, 2) ? 0 : 1;
}
