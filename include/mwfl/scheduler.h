#pragma once
#include <chrono>
#include <filesystem>
#include <mwfl/core.h>
#include <string>
#include <vector>
namespace mwfl {
enum class ScheduledTaskTrigger { OnDemand, CurrentUserLogon, Once };
struct TaskDefinition {
    std::wstring folder = L"\\mwfl";
    std::wstring name;
    std::wstring description;
    std::filesystem::path executable;
    std::vector<std::wstring> arguments;
    ScheduledTaskTrigger trigger = ScheduledTaskTrigger::OnDemand;
    std::chrono::system_clock::time_point start_time{};
};
struct ScheduledTaskSnapshot {
    bool exists = false;
    std::wstring folder;
    std::wstring name;
    LONG state = 0;
    bool enabled = false;
    std::wstring description;
    std::filesystem::path executable;
    std::wstring arguments;
    ScheduledTaskTrigger trigger = ScheduledTaskTrigger::OnDemand;
    std::wstring start_boundary;
};
struct TaskApplyResult {
    bool created = false;
    bool changed = false;
    ScheduledTaskSnapshot snapshot;
};
struct TaskChangePlan {
    TaskDefinition desired;
    ScheduledTaskSnapshot current;
    bool required = false;
    bool creates = false;
};
class TaskScheduler final {
   public:
    [[nodiscard]] Result<ScheduledTaskSnapshot> Query(std::wstring_view folder,
                                                      std::wstring_view name) const;
    [[nodiscard]] Result<TaskChangePlan> Plan(const TaskDefinition& definition) const;
    [[nodiscard]] Result<TaskApplyResult> Apply(const TaskChangePlan& plan) const;
    [[nodiscard]] Result<void> Run(std::wstring_view folder, std::wstring_view name) const;
    [[nodiscard]] Result<void> Stop(std::wstring_view folder, std::wstring_view name) const;
    [[nodiscard]] Result<bool> Remove(std::wstring_view folder, std::wstring_view name) const;
};
[[nodiscard]] Result<void> ValidateScheduledTask(const TaskDefinition& spec);
}  // namespace mwfl
