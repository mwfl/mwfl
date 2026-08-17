#pragma once
#include <mwfl/core.h>
#include <chrono>
#include <functional>
#include <stop_token>
#include <string>
#include <string_view>
namespace mwfl {
enum class ServiceState { Stopped, StartPending, Running, PausePending, Paused, ContinuePending, StopPending };
class ServiceStateMachine final {
public:
    [[nodiscard]] ServiceState State() const noexcept { return state_; }
    [[nodiscard]] bool Transition(ServiceState next) noexcept;
private:
    ServiceState state_ = ServiceState::Stopped;
};
struct ServiceDefinition {
    std::wstring name;
    std::wstring display_name;
    std::chrono::milliseconds stop_timeout{30000};
};
using ServiceMain = std::function<Result<void>(std::stop_token)>;
[[nodiscard]] int RunServiceConsole(const ServiceDefinition& definition, ServiceMain main);
// Connects the process to the Service Control Manager. The process hosts one
// SERVICE_WIN32_OWN_PROCESS service and contains all callback exceptions.
[[nodiscard]] int RunWindowsService(const ServiceDefinition& definition, ServiceMain main);
}  // namespace mwfl
