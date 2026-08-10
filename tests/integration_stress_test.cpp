#include <mwfl/file_association.h>
#include <mwfl/graphics.h>
#include <mwfl/help.h>
#include <mwfl/mdi.h>
#include <mwfl/ole_data.h>
#include <mwfl/printing_native.h>
#include <mwfl/ribbon.h>
#include <mwfl/settings_store.h>
#include <mwfl/shell_integration.h>

#include <array>
#include <cstdio>
#include <memory>
#include <filesystem>
#include <string>

namespace {
class StressPrintBackend final : public mwfl::PrintBackend {
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
    using namespace mwfl;
    if (FAILED(::OleInitialize(nullptr))) return 1;
    struct OleCleanup { ~OleCleanup() { ::OleUninitialize(); } } ole_cleanup;

    WNDCLASSW type{};
    type.lpfnWndProc = WindowProcedure;
    type.hInstance = ::GetModuleHandleW(nullptr);
    type.lpszClassName = L"mwfl.IntegrationStress";
    if (!::RegisterClassW(&type) && ::GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return 2;
    HWND window = ::CreateWindowExW(0, type.lpszClassName, L"", WS_OVERLAPPED,
                                     0, 0, 64, 64, nullptr, nullptr, type.hInstance, nullptr);
    if (!window) return 3;

    const DWORD gdi_before = ::GetGuiResources(::GetCurrentProcess(), GR_GDIOBJECTS);
    const DWORD user_before = ::GetGuiResources(::GetCurrentProcess(), GR_USEROBJECTS);
    const std::wstring root = L"Software\\mwfl\\Tests\\IntegrationStress-" +
                              std::to_wstring(::GetCurrentProcessId());
    const std::wstring classes = root + L"\\Classes";
    const std::wstring settings_key = root + L"\\Settings";
    const auto pages = PaginateContent(41, 10);
    const std::array<JumpListTask, 2> tasks{
        JumpListTask{L"one", L"One", {}, {}, 0},
        JumpListTask{L"two", L"Two", {}, {}, 0}};
    const std::array<std::wstring, 1> removed{L"one"};
    FileAssociationSpec association{
        .extension = L".stress", .prog_id = L"mwfl.tests.stress.document",
        .owner_id = L"mwfl.tests.stress", .display_name = L"Stress document",
        .executable = L"C:\\Windows\\notepad.exe", .icon = L"C:\\Windows\\notepad.exe",
        .verbs = {{L"open", L"Open", {}}}};
    VersionedSettingsStore settings{HKEY_CURRENT_USER, settings_key, 1};
    const auto png = std::filesystem::temp_directory_path() /
        (L"mwfl-integration-stress-" + std::to_wstring(::GetCurrentProcessId()) + L".png");
    std::error_code ignored;
    std::filesystem::remove(png, ignored);

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

        MdiWorkspaceModel mdi_model;
        const MdiChildId child{static_cast<std::uint64_t>(iteration + 1)};
        if (!mdi_model.Add({child, L"Stress child"})) return 16;
        MdiHost mdi;
        if (!mdi.Create(window, mdi_model) || !mdi.CreateChild(child) ||
            !mdi.Arrange(MdiArrange::cascade) || !mdi.Close(child)) return 17;
        mdi.Destroy();

        CommandSet commands;
        commands.Add(Command({1}, L"Stress", [] {}));
        RibbonCommandModel ribbon_model;
        if (!ribbon_model.Add({{1}, {1}})) return 18;
        RibbonFrameworkHost ribbon;
        const auto ribbon_created = ribbon.Create(window, ribbon_model, commands);
        if (ribbon_created) {
            ribbon.Destroy();
        } else if (ribbon_created.status != RibbonHostStatus::unavailable &&
                   ribbon_created.status != RibbonHostStatus::com_failure)
            return 20;

        auto metafile = RecordEnhancedMetafile(nullptr, L"stress", [](HDC dc) {
            ::Rectangle(dc, 0, 0, 32, 32);
        });
        if (!metafile) return 21;
        GdiPlusSession graphics;
        if (!graphics.Start() || !graphics.Stop()) return 22;
        if (iteration % 25 == 0) {
            if (!ExportGdiPlusPng(png, 16, 16, [](Gdiplus::Graphics& target) {
                    target.Clear(Gdiplus::Color(255, 10, 20, 30));
                })) return 23;
            std::filesystem::remove(png, ignored);
        }

        const HelpRequest help{HelpTargetKind::https_uri, {},
                               L"https://example.invalid/help", L"#stress"};
        const auto help_result = LaunchHelpWithBackend(window, help,
            [](HWND, const HelpRequest&) {
                return HelpResult{HelpStatus::cancelled, ERROR_CANCELLED};
            });
        if (help_result.status != HelpStatus::cancelled) return 24;
    }

    ::RegDeleteTreeW(HKEY_CURRENT_USER, root.c_str());
    HKEY leftover = nullptr;
    if (::RegOpenKeyExW(HKEY_CURRENT_USER, root.c_str(), 0, KEY_READ, &leftover) == ERROR_SUCCESS) {
        ::RegCloseKey(leftover);
        return 13;
    }
    if (!::DestroyWindow(window)) return 14;
    if (std::filesystem::exists(png)) return 25;
    const DWORD gdi_after = ::GetGuiResources(::GetCurrentProcess(), GR_GDIOBJECTS);
    const DWORD user_after = ::GetGuiResources(::GetCurrentProcess(), GR_USEROBJECTS);
    if (gdi_after > gdi_before + 4 || user_after > user_before + 4) {
        std::printf("resource delta: GDI %ld, USER %ld\n",
            static_cast<long>(gdi_after) - static_cast<long>(gdi_before),
            static_cast<long>(user_after) - static_cast<long>(user_before));
        return 15;
    }
    return 0;
}
