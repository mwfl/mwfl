#include <fstream>
#include <mwfl/deployment.h>
#include <string>
int main() {
    auto identity = mwfl::QueryCurrentPackageIdentity();
    if (!identity) return 1;
    std::wstring too_long(RESTART_MAX_CMD_LINE, L'x');
    auto invalid = mwfl::RegisterApplicationRestart(too_long);
    if (invalid) return 2;
    wchar_t executable[MAX_PATH]{};
    if (!GetModuleFileNameW(nullptr, executable, MAX_PATH)) return 3;
    auto version = mwfl::QueryFileVersion(executable);
    if (!version && version.GetError().code == ERROR_SUCCESS) return 4;
    auto signature = mwfl::VerifyAuthenticode(executable);
    if (!signature) return 5;
    auto recovery = mwfl::RecoveryRegistration::Register(
        [](std::stop_token) -> mwfl::Result<void> { return {}; }, std::chrono::seconds(5));
    if (!recovery || !recovery.Value().Active()) return 6;

    const auto root = std::filesystem::temp_directory_path();
    const auto suffix = std::to_wstring(GetCurrentProcessId());
    const auto candidate = root / (L"mwfl-deployment-candidate-" + suffix + L".bin");
    const auto target = root / (L"mwfl-deployment-target-" + suffix + L".bin");
    const auto backup = root / (L"mwfl-deployment-backup-" + suffix + L".bin");
    {
        std::ofstream(candidate, std::ios::binary) << "new";
    }
    {
        std::ofstream(target, std::ios::binary) << "old";
    }
    mwfl::UpdateVerificationPolicy policy;
    policy.require_valid_signature = false;
    auto plan = mwfl::VerifyUpdate(candidate, target, backup, target, {}, policy);
    if (!plan) return 7;
    auto staged = mwfl::StageUpdate(std::move(plan.Value()));
    if (!staged) return 8;
    auto applied = mwfl::ApplyUpdate(std::move(staged.Value()), 0,
                                     mwfl::Deadline::After(std::chrono::seconds(1)));
    if (applied) return 9;
    std::string restored;
    {
        std::ifstream input(target, std::ios::binary);
        input >> restored;
    }
    if (restored != "old") return 10;
    std::filesystem::remove(candidate);
    std::filesystem::remove(target);
    std::filesystem::remove(backup);
    return 0;
}
