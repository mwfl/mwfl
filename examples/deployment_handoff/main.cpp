#include <mwfl/deployment.h>
#include <fstream>
#include <iostream>
int wmain(int argc, wchar_t** argv) {
    if (argc != 2 || std::wstring_view(argv[1]) != L"--self-test") return 0;
    const auto root = std::filesystem::temp_directory_path(); const auto suffix = std::to_wstring(GetCurrentProcessId()); const auto candidate = root / (L"mwfl-candidate-" + suffix + L".bin"); const auto target = root / (L"mwfl-target-" + suffix + L".bin"); const auto backup = root / (L"mwfl-target-" + suffix + L".bak");
    { std::ofstream output(candidate, std::ios::binary); output << "candidate"; }
    { std::ofstream output(target, std::ios::binary); output << "target"; }
    mwfl::UpdateVerificationPolicy policy; policy.require_valid_signature = false;
    auto plan = mwfl::VerifyUpdate(candidate, target, backup, target, {}, policy); std::filesystem::remove(candidate); std::filesystem::remove(target); std::filesystem::remove(backup);
    if (!plan || plan.Value().Target().filename() != target.filename()) return 1;
    std::wcout << L"verified update handoff planning passed\n"; return 0;
}
