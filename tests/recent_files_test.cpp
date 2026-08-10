#include <mwfl/recent_files.h>

#include <windows.h>

#include <cstdlib>
#include <cstdio>
#include <stdexcept>
#include <string>

namespace {
#define MWFL_CHECK(condition) do { if (!(condition)) { \
    std::fprintf(stderr, "recent_files_test failed at line %d: %s\n", __LINE__, #condition); \
    return EXIT_FAILURE; } } while (false)
class TestRegistryKey final {
public:
    TestRegistryKey() : subkey_(L"Software\\mwfl\\Tests\\RecentFiles-" +
        std::to_wstring(::GetCurrentProcessId()) + L"-" + std::to_wstring(::GetTickCount64())) {}
    ~TestRegistryKey() noexcept { ::RegDeleteTreeW(HKEY_CURRENT_USER, subkey_.c_str()); }
    const std::wstring& Get() const noexcept { return subkey_; }
private:
    std::wstring subkey_;
};
}  // namespace

int main() {
    try { mwfl::RecentFileList invalid{0}; return EXIT_FAILURE; }
    catch (const std::invalid_argument&) {}
    mwfl::RecentFileList files{3};
    MWFL_CHECK(files.Add(LR"(C:\One.txt)") && files.Add(LR"(C:\Two.txt)") &&
        files.Add(LR"(C:\Three.txt)") && files.GetPaths().size() == 3);
    MWFL_CHECK(files.Add(LR"(c:\ONE.TXT)") && files.GetPaths().front() == LR"(c:\ONE.TXT)" &&
        files.GetPaths().size() == 3);
    MWFL_CHECK(!files.Add(LR"(c:\ONE.TXT)") && files.Remove(LR"(C:\two.TXT)") &&
        !files.Remove(LR"(C:\missing.txt)"));

    TestRegistryKey key;
    if (mwfl::LoadRecentFilesFromRegistry(HKEY_CURRENT_USER, key.Get(), 3).status !=
        mwfl::SettingsStatus::not_found) MWFL_CHECK(false);
    const auto saved = mwfl::SaveRecentFilesToRegistry(HKEY_CURRENT_USER, key.Get(), files);
    if (saved.status == mwfl::SettingsStatus::access_denied) {
        std::fprintf(stderr, "registry persistence skipped: test environment denied isolated HKCU writes\n");
        return 77;
    }
    if (!saved.Succeeded()) {
        std::fprintf(stderr, "initial registry save failed: status=%d native=%ld\n",
                     static_cast<int>(saved.status), saved.native_error);
        return EXIT_FAILURE;
    }
    const auto loaded = mwfl::LoadRecentFilesFromRegistry(HKEY_CURRENT_USER, key.Get(), 3);
    if (!loaded.Succeeded() || !loaded.value ||
        loaded.value->GetPaths().size() != files.GetPaths().size()) MWFL_CHECK(false);
    for (std::size_t index = 0; index < files.GetPaths().size(); ++index)
        MWFL_CHECK(loaded.value->GetPaths()[index] == files.GetPaths()[index]);

    mwfl::RecentFileList empty{3};
    if (!mwfl::SaveRecentFilesToRegistry(HKEY_CURRENT_USER, key.Get(), empty).Succeeded())
        MWFL_CHECK(false);
    const auto loaded_empty = mwfl::LoadRecentFilesFromRegistry(HKEY_CURRENT_USER, key.Get(), 3);
    MWFL_CHECK(loaded_empty.Succeeded() && loaded_empty.value->GetPaths().empty());

    HKEY raw{};
    if (::RegOpenKeyExW(HKEY_CURRENT_USER, key.Get().c_str(), 0, KEY_SET_VALUE, &raw) != ERROR_SUCCESS)
        MWFL_CHECK(false);
    const DWORD bad_schema = 99;
    const LSTATUS write_error = ::RegSetValueExW(raw, L"SchemaVersion", 0, REG_DWORD,
        reinterpret_cast<const BYTE*>(&bad_schema), sizeof(bad_schema));
    ::RegCloseKey(raw);
    if (write_error != ERROR_SUCCESS ||
        mwfl::LoadRecentFilesFromRegistry(HKEY_CURRENT_USER, key.Get(), 3).status !=
            mwfl::SettingsStatus::invalid_data) MWFL_CHECK(false);
    return EXIT_SUCCESS;
}
