#include <mwfl/shell_integration.h>

#include <array>
#include <atomic>
#include <string>
#include <thread>

namespace {
LRESULT CALLBACK Procedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    return ::DefWindowProcW(window, message, wparam, lparam);
}
}

int main() {
    using namespace mwfl;
    TaskbarProgressModel progress;
    if (!progress.IsValid() || progress.SetValue(2, 1) || !progress.SetValue(1, 4) ||
        progress.state != TaskbarProgressState::normal ||
        !progress.SetState(TaskbarProgressState::paused) || !progress.IsValid()) return 1;
    progress.Reset();
    if (progress.state != TaskbarProgressState::none || !progress.IsValid()) return 2;

    const std::array tasks{
        JumpListTask{L"new", L"New document", L"--new"},
        JumpListTask{L"settings", L"Settings", L"--settings"},
        JumpListTask{L"help", L"Help", L"--help"}};
    const std::array<std::wstring, 1> removed{L"settings"};
    const auto plan = BuildJumpListPlan(tasks, removed, 2);
    if (!plan.valid || plan.tasks.size() != 2 || plan.tasks[0].id != L"new" ||
        plan.tasks[1].id != L"help") return 3;
    auto duplicate = tasks;
    duplicate[1].id = L"new";
    if (BuildJumpListPlan(duplicate, {}, 3).valid || BuildJumpListPlan(tasks, {}, 0).valid)
        return 4;

    const HRESULT initialized = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(initialized)) return 5;
    struct Uninitialize { ~Uninitialize() { ::CoUninitialize(); } } uninitialize;
    WNDCLASSW type{};
    type.lpfnWndProc = Procedure;
    type.hInstance = ::GetModuleHandleW(nullptr);
    type.lpszClassName = L"mwfl.ShellIntegrationTest";
    if (!::RegisterClassW(&type) && ::GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return 6;
    HWND window = ::CreateWindowExW(0, type.lpszClassName, L"", WS_OVERLAPPED,
                                     0, 0, 100, 100, nullptr, nullptr, type.hInstance, nullptr);
    if (!window) return 7;
    TaskbarWindowIntegration taskbar;
    const auto created = taskbar.Create(window);
    if (created) {
        progress.SetValue(2, 5);
        if (!taskbar.Apply(progress) || !taskbar.SetOverlayIcon(nullptr, L"clear") ||
            !taskbar.Clear() || !taskbar.Recreate()) return 8;
        std::atomic<ShellStatus> cross_thread{ShellStatus::success};
        std::thread worker([&] { cross_thread = taskbar.Apply(progress).status; });
        worker.join();
        if (cross_thread != ShellStatus::wrong_thread) return 14;
        const std::array thumbnail_buttons{
            TaskbarThumbnailButton{1, nullptr, L"Previous", THBF_ENABLED},
            TaskbarThumbnailButton{2, nullptr, L"Next", THBF_ENABLED}};
        const auto thumbnail = taskbar.SetThumbnailButtons(thumbnail_buttons);
        if (!thumbnail && thumbnail.status != ShellStatus::com_failure &&
            thumbnail.status != ShellStatus::rejected) return 15;
        auto duplicate_buttons = thumbnail_buttons;
        duplicate_buttons[1].id = 1;
        if (taskbar.SetThumbnailButtons(duplicate_buttons).status !=
            ShellStatus::invalid_argument) return 16;
        HWND tab = ::CreateWindowExW(0, type.lpszClassName, L"tab", WS_OVERLAPPED,
            0, 0, 100, 100, nullptr, nullptr, type.hInstance, nullptr);
        if (!tab) return 17;
        const auto registered = taskbar.RegisterTab(tab);
        if (registered) {
            if (!taskbar.SetActiveTab(tab) || !taskbar.UnregisterTab(tab)) return 18;
        } else if (registered.status != ShellStatus::com_failure &&
                   registered.status != ShellStatus::rejected) return 19;
        ::DestroyWindow(tab);
    } else if (created.status != ShellStatus::unavailable &&
               created.status != ShellStatus::com_failure) return 9;
    if (TaskbarWindowIntegration::GetTaskbarCreatedMessage() == 0) return 10;
    taskbar.Reset();
    ::DestroyWindow(window);

    const std::wstring app_id = L"mwfl.Tests.JumpList." +
                                std::to_wstring(::GetCurrentProcessId());
    JumpListCommitOptions options{app_id, L"C:\\Windows\\System32\\notepad.exe",
                                  std::vector<JumpListTask>(tasks.begin(), tasks.end())};
    const auto committed = CommitJumpList(options);
    if (committed) {
        const auto deleted = DeleteJumpList(app_id);
        if (!deleted) return 11;
    } else if (committed.status != ShellStatus::unavailable &&
               committed.status != ShellStatus::rejected &&
               committed.status != ShellStatus::com_failure) return 12;
    if (SetProcessAppUserModelId(L"").status != ShellStatus::invalid_argument ||
        AddRecentDocument({}).status != ShellStatus::invalid_argument) return 13;
    return 0;
}
