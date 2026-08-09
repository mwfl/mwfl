#pragma once

#include <windows.h>
#include <shobjidl_core.h>
#include <wrl/client.h>

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace mwtl {

enum class ShellStatus {
    success,
    unavailable,
    wrong_apartment,
    wrong_thread,
    invalid_argument,
    rejected,
    com_failure,
};

struct ShellResult {
    ShellStatus status = ShellStatus::com_failure;
    HRESULT error = E_FAIL;
    explicit operator bool() const noexcept { return status == ShellStatus::success; }
};

enum class TaskbarProgressState { none, indeterminate, normal, error, paused };

struct TaskbarProgressModel {
    TaskbarProgressState state = TaskbarProgressState::none;
    std::uint64_t completed = 0;
    std::uint64_t total = 0;
    bool SetState(TaskbarProgressState value) noexcept;
    bool SetValue(std::uint64_t value, std::uint64_t maximum) noexcept;
    void Reset() noexcept;
    bool IsValid() const noexcept;
};

// UI-thread object. Recreate after the registered TaskbarCreated message.
class TaskbarWindowIntegration final {
public:
    TaskbarWindowIntegration() noexcept = default;
    ShellResult Create(HWND window) noexcept;
    ShellResult Recreate() noexcept;
    ShellResult Apply(const TaskbarProgressModel& progress) noexcept;
    // icon is borrowed for the call. The Shell copies it. nullptr clears.
    ShellResult SetOverlayIcon(HICON icon, std::wstring_view accessible_description) noexcept;
    ShellResult Clear() noexcept;
    void Reset() noexcept;
    bool IsAvailable() const noexcept { return taskbar_ != nullptr; }
    HWND GetWindow() const noexcept { return window_; }
    static UINT GetTaskbarCreatedMessage() noexcept;

private:
    ShellResult CheckThread() const noexcept;
    HWND window_ = nullptr;
    DWORD thread_id_ = 0;
    Microsoft::WRL::ComPtr<ITaskbarList3> taskbar_;
};

struct JumpListTask {
    std::wstring id;
    std::wstring title;
    std::wstring arguments;
    std::filesystem::path icon;
    int icon_index = 0;
};

struct JumpListPlan {
    std::vector<JumpListTask> tasks;
    std::size_t maximum_slots = 0;
    bool valid = false;
};

// Pure filtering. removed_ids are stable application-defined task IDs read
// back from the Shell link arguments. Order is preserved and slots are bounded.
JumpListPlan BuildJumpListPlan(std::span<const JumpListTask> tasks,
                               std::span<const std::wstring> removed_ids,
                               std::size_t maximum_slots);

struct JumpListCommitOptions {
    std::wstring app_id;
    std::filesystem::path executable;
    std::vector<JumpListTask> tasks;
};

// Requires an STA. Tasks use --mwtl-jump-task=<id> as a stable removed-item
// identity. Failed transactions call AbortList. Delete is provided for cleanup.
ShellResult CommitJumpList(const JumpListCommitOptions& options) noexcept;
ShellResult DeleteJumpList(std::wstring_view app_id) noexcept;
ShellResult AddRecentDocument(const std::filesystem::path& path) noexcept;
ShellResult SetProcessAppUserModelId(std::wstring_view app_id) noexcept;
ShellResult SetWindowAppUserModelId(HWND window, std::wstring_view app_id) noexcept;

}  // namespace mwtl
