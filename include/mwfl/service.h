#pragma once
#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <mwfl/core.h>
#include <stop_token>
#include <string>
#include <vector>
namespace mwfl {
enum class ServiceState {
    Stopped,
    StartPending,
    Running,
    PausePending,
    Paused,
    ContinuePending,
    StopPending
};
class ServiceStateMachine final {
   public:
    [[nodiscard]] ServiceState State() const noexcept { return state_; }
    [[nodiscard]] bool Transition(ServiceState next) noexcept;

   private:
    ServiceState state_ = ServiceState::Stopped;
};
enum class ServiceControlKind { Stop, Shutdown, Pause, Continue, Power, Session, Custom };
struct ServiceControlEvent {
    ServiceControlKind kind = ServiceControlKind::Stop;
    DWORD native_control = 0;
    DWORD event_type = 0;
    const void* event_data = nullptr;
};
struct ServiceExit {
    DWORD win32_code = ERROR_SUCCESS;
    DWORD service_specific_code = 0;
};
struct ServiceDefinition {
    std::wstring name;
    std::wstring display_name;
    std::chrono::milliseconds stop_timeout{30000};
    bool supports_pause = false;
};
class ServiceContext final {
   public:
    [[nodiscard]] std::stop_token StopToken() const noexcept;
    [[nodiscard]] bool IsPaused() const noexcept;
    [[nodiscard]] WaitStatus WaitWhilePaused(
        std::chrono::milliseconds timeout = std::chrono::milliseconds{-1}) const;

   private:
    explicit ServiceContext(std::shared_ptr<void> state) : state_(std::move(state)) {}
    std::shared_ptr<void> state_;
    friend struct ServiceContextAccess;
};
struct ServiceCallbacks {
    std::function<Result<ServiceExit>(ServiceContext&)> run;
    std::function<Result<void>(ServiceContext&, const ServiceControlEvent&)> control;
    std::function<void(ServiceState)> state_changed;
};
using ServiceMain = std::function<Result<void>(std::stop_token)>;
[[nodiscard]] int RunServiceConsole(const ServiceDefinition& definition,
                                    ServiceCallbacks callbacks);
[[nodiscard]] int RunWindowsService(const ServiceDefinition& definition,
                                    ServiceCallbacks callbacks);
[[nodiscard]] int RunServiceConsole(const ServiceDefinition& definition, ServiceMain main);
[[nodiscard]] int RunWindowsService(const ServiceDefinition& definition, ServiceMain main);
enum class ServiceAccount { LocalSystem, LocalService, NetworkService };
struct ServiceInstallSpec {
    std::wstring name;
    std::wstring display_name;
    std::filesystem::path executable;
    std::vector<std::wstring> arguments;
    ServiceAccount account = ServiceAccount::LocalService;
    DWORD start_type = SERVICE_DEMAND_START;
};
struct ServiceSnapshot {
    std::wstring name;
    std::wstring display_name;
    std::wstring binary_path;
    std::wstring account_name;
    DWORD start_type = SERVICE_DEMAND_START;
    DWORD state = SERVICE_STOPPED;
    DWORD process_id = 0;
    DWORD win32_exit_code = 0;
    DWORD service_specific_exit_code = 0;
};
enum class ServiceQueryStatus { Found, NotFound };
struct ServiceQueryResult {
    ServiceQueryStatus status = ServiceQueryStatus::NotFound;
    ServiceSnapshot snapshot;
};
struct ServiceMutationResult {
    bool created = false;
    bool changed = false;
    std::vector<std::wstring> changed_fields;
    ServiceSnapshot snapshot;
};
enum class ServiceOperationStatus { ReachedTarget, TimedOut, Cancelled };
struct ServiceOperationResult {
    ServiceOperationStatus status = ServiceOperationStatus::ReachedTarget;
    ServiceSnapshot snapshot;
};
class ServiceManager final {
   public:
    ServiceManager() = default;
    ~ServiceManager();
    ServiceManager(ServiceManager&& other) noexcept;
    ServiceManager& operator=(ServiceManager&& other) noexcept;
    ServiceManager(const ServiceManager&) = delete;
    ServiceManager& operator=(const ServiceManager&) = delete;
    enum class Access { Query, Manage };
    [[nodiscard]] static Result<ServiceManager> Open(Access access = Access::Query);
    [[nodiscard]] Result<ServiceQueryResult> Query(std::wstring_view name) const;
    [[nodiscard]] Result<ServiceMutationResult> InstallOrUpdate(
        const ServiceInstallSpec& spec) const;
    [[nodiscard]] Result<ServiceOperationResult> Start(std::wstring_view name,
                                                       std::chrono::milliseconds timeout,
                                                       std::stop_token stop = {}) const;
    [[nodiscard]] Result<ServiceOperationResult> Stop(std::wstring_view name,
                                                      std::chrono::milliseconds timeout,
                                                      std::stop_token stop = {}) const;
    [[nodiscard]] Result<ServiceSnapshot> SendControl(std::wstring_view name, DWORD control) const;
    [[nodiscard]] Result<bool> Remove(std::wstring_view name) const;

   private:
    explicit ServiceManager(SC_HANDLE manager, Access access)
        : manager_(manager), access_(access) {}
    SC_HANDLE manager_ = nullptr;
    Access access_ = Access::Query;
};
}  // namespace mwfl
