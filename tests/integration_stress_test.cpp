#include <mwtl/file_association.h>
#include <mwtl/ole_data.h>
#include <mwtl/printing_native.h>
#include <mwtl/settings_store.h>
#include <mwtl/shell_integration.h>

#include <array>
#include <memory>
#include <string>

namespace {
class StressPrintBackend final : public mwtl::PrintBackend {
public:
    int StartDocument(std::wstring_view) noexcept override { return 1; }
    int StartPage() noexcept override { return 1; }
    int EndPage() noexcept override { return 1; }
    int EndDocument() noexcept override { return 1; }
    int AbortDocument() noexcept override { return 1; }
    HDC DeviceContext() const noexcept override { return reinterpret_cast<HDC>(1); }
    DWORD LastError() const noexcept override { return ERROR_SUCCESS; }
};

LRESULT CALLBACK WindowProcedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    return ::DefWindowProcW(window, message, wparam, lparam);
}
}  // namespace

int main() {
    using namespace mwtl;
    if (FAILED(::OleInitialize(nullptr))) return 1;
    struct OleCleanup { ~OleCleanup() { ::OleUninitialize(); } } ole_cleanup;

    WNDCLASSW type{};
    type.lpfnWndProc = WindowProcedure;
    type.hInstance = ::GetModuleHandleW(nullptr);
    type.lpszClassName = L"mwtl.IntegrationStress";
    if (!::RegisterClassW(&type) && ::GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return 2;
    HWND window = ::CreateWindowExW(0, type.lpszClassName, L"", WS_OVERLAPPED,
                                     0, 0, 64, 64, nullptr, nullptr, type.hInstance, nullptr);
    if (!window) return 3;

    const DWORD gdi_before = ::GetGuiResources(::GetCurrentProcess(), GR_GDIOBJECTS);
    const DWORD user_before = ::GetGuiResources(::GetCurrentProcess(), GR_USEROBJECTS);
    const std::wstring root = L"Software\\mwtl\\Tests\\IntegrationStress-" +
                              std::to_wstring(::GetCurrentProcessId());
    const std::wstring classes = root + L"\\Classes";
    const std::wstring settings_key = root + L"\\Settings";
    const auto pages = PaginateContent(41, 10);
    const std::array<JumpListTask, 2> tasks{
        JumpListTask{L"one", L"One", {}, {}, 0},
        JumpListTask{L"two", L"Two", {}, {}, 0}};
    const std::array<std::wstring, 1> removed{L"one"};
    FileAssociationSpec association{
        .extension = L".stress", .prog_id = L"mwtl.tests.stress.document",
        .owner_id = L"mwtl.tests.stress", .display_name = L"Stress document",
        .executable = L"C:\\Windows\\notepad.exe", .icon = L"C:\\Windows\\notepad.exe",
        .verbs = {{L"open", L"Open", {}}}};
    VersionedSettingsStore settings{HKEY_CURRENT_USER, settings_key, 1};

    for (int iteration = 0; iteration < 200; ++iteration) {
        PrintJob job{std::make_unique<StressPrintBackend>()};
        if (PrintPages(job, L"stress", pages,
                       [](HDC dc, const PrintPage&) { return dc != nullptr; }) !=
            PrintOperationStatus::success) return 4;
        OleDataObjectBuilder builder{4096};
        if (!builder.AddUnicodeText(L"stress") || !builder.Build()) return 5;
        const auto plan = BuildJumpListPlan(tasks, removed, 10);
        if (!plan.valid || plan.tasks.size() != 1) return 6;

        TaskbarWindowIntegration taskbar;
        const auto created = taskbar.Create(window);
        if (created.status != ShellStatus::success && created.status != ShellStatus::unavailable &&
            created.status != ShellStatus::com_failure) return 7;
        TaskbarProgressModel progress;
        progress.SetValue(static_cast<std::uint64_t>(iteration % 101), 100);
        if (created && (!taskbar.Apply(progress) || !taskbar.Clear())) return 8;

        const std::array values{
            SettingValue{L"Iteration", static_cast<std::uint32_t>(iteration)}};
        if (!settings.Save(values)) return 9;
        const std::array<std::wstring_view, 1> owned{L"Iteration"};
        if (!settings.RemoveOwned(owned)) return 10;
        if (!RegisterFileAssociation(HKEY_CURRENT_USER, classes, association, false)) return 11;
        if (!RemoveFileAssociation(HKEY_CURRENT_USER, classes, association, false)) return 12;
    }

    ::RegDeleteTreeW(HKEY_CURRENT_USER, root.c_str());
    HKEY leftover = nullptr;
    if (::RegOpenKeyExW(HKEY_CURRENT_USER, root.c_str(), 0, KEY_READ, &leftover) == ERROR_SUCCESS) {
        ::RegCloseKey(leftover);
        return 13;
    }
    if (!::DestroyWindow(window)) return 14;
    const DWORD gdi_after = ::GetGuiResources(::GetCurrentProcess(), GR_GDIOBJECTS);
    const DWORD user_after = ::GetGuiResources(::GetCurrentProcess(), GR_USEROBJECTS);
    if (gdi_after > gdi_before + 2 || user_after > user_before + 2) return 15;
    return 0;
}
