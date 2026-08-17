#include <mwfl/service.h>
#include <algorithm>
#include <atomic>
#include <iostream>
#include <mutex>
namespace mwfl { namespace {
std::mutex handler_mutex;
std::stop_source* active_stop = nullptr;
const ServiceDefinition* active_definition = nullptr;
ServiceMain* active_main = nullptr;
SERVICE_STATUS_HANDLE active_status_handle = nullptr;
SERVICE_STATUS active_status{};
BOOL WINAPI ConsoleHandler(DWORD control) noexcept {
    if (control != CTRL_C_EVENT && control != CTRL_BREAK_EVENT && control != CTRL_CLOSE_EVENT && control != CTRL_SHUTDOWN_EVENT) return FALSE;
    std::scoped_lock lock(handler_mutex);
    if (active_stop != nullptr) active_stop->request_stop();
    return TRUE;
}
void ReportServiceStatus(DWORD state, DWORD win32_exit = NO_ERROR,
                         DWORD checkpoint = 0, DWORD wait_hint = 0) noexcept {
    if (active_status_handle == nullptr) return;
    active_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    active_status.dwCurrentState = state;
    active_status.dwWin32ExitCode = win32_exit;
    active_status.dwServiceSpecificExitCode = 0;
    active_status.dwCheckPoint = checkpoint;
    active_status.dwWaitHint = wait_hint;
    active_status.dwControlsAccepted =
        state == SERVICE_RUNNING ? SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN |
                                       SERVICE_ACCEPT_PAUSE_CONTINUE
                                 : 0;
    SetServiceStatus(active_status_handle, &active_status);
}
DWORD WINAPI ServiceControl(DWORD control, DWORD, void*, void*) noexcept {
    try {
        std::scoped_lock lock(handler_mutex);
        if (active_stop == nullptr) return NO_ERROR;
        switch (control) {
        case SERVICE_CONTROL_STOP:
        case SERVICE_CONTROL_SHUTDOWN:
            ReportServiceStatus(SERVICE_STOP_PENDING, NO_ERROR, 1,
                                active_definition == nullptr
                                    ? 30000
                                    : static_cast<DWORD>((std::min)(
                                          active_definition->stop_timeout.count(), 120000LL)));
            active_stop->request_stop();
            break;
        case SERVICE_CONTROL_INTERROGATE:
            SetServiceStatus(active_status_handle, &active_status);
            break;
        default:
            break;
        }
        return NO_ERROR;
    } catch (...) {
        return ERROR_EXCEPTION_IN_SERVICE;
    }
}
void WINAPI ServiceEntry(DWORD, wchar_t**) noexcept {
    DWORD exit_code = ERROR_EXCEPTION_IN_SERVICE;
    try {
        active_status_handle = RegisterServiceCtrlHandlerExW(
            active_definition->name.c_str(), ServiceControl, nullptr);
        if (active_status_handle == nullptr) return;
        ReportServiceStatus(SERVICE_START_PENDING, NO_ERROR, 1, 10000);
        std::stop_source stop;
        {
            std::scoped_lock lock(handler_mutex);
            active_stop = &stop;
        }
        ReportServiceStatus(SERVICE_RUNNING);
        Result<void> result = (*active_main)(stop.get_token());
        exit_code = result ? NO_ERROR : result.Error().code;
        {
            std::scoped_lock lock(handler_mutex);
            active_stop = nullptr;
        }
    } catch (...) {
        exit_code = ERROR_EXCEPTION_IN_SERVICE;
    }
    ReportServiceStatus(SERVICE_STOPPED, exit_code);
    active_status_handle = nullptr;
}
bool Allowed(ServiceState from, ServiceState to) noexcept {
    switch (from) {
    case ServiceState::Stopped: return to == ServiceState::StartPending;
    case ServiceState::StartPending: return to == ServiceState::Running || to == ServiceState::StopPending;
    case ServiceState::Running: return to == ServiceState::PausePending || to == ServiceState::StopPending;
    case ServiceState::PausePending: return to == ServiceState::Paused || to == ServiceState::StopPending;
    case ServiceState::Paused: return to == ServiceState::ContinuePending || to == ServiceState::StopPending;
    case ServiceState::ContinuePending: return to == ServiceState::Running || to == ServiceState::StopPending;
    case ServiceState::StopPending: return to == ServiceState::Stopped;
    }
    return false;
}
}  // namespace
bool ServiceStateMachine::Transition(ServiceState next) noexcept { if (!Allowed(state_, next)) return false; state_ = next; return true; }
int RunServiceConsole(const ServiceDefinition& definition, ServiceMain main) {
    if (definition.name.empty() || !main) return ERROR_INVALID_PARAMETER;
    std::stop_source stop;
    {
        std::scoped_lock lock(handler_mutex); active_stop = &stop;
    }
    if (!SetConsoleCtrlHandler(ConsoleHandler, TRUE)) {
        std::scoped_lock lock(handler_mutex); active_stop = nullptr;
        return static_cast<int>(GetLastError());
    }
    Result<void> result;
    try { result = main(stop.get_token()); }
    catch (...) { result = NativeError{ErrorDomain::Application, ERROR_UNHANDLED_EXCEPTION}; }
    SetConsoleCtrlHandler(ConsoleHandler, FALSE);
    {
        std::scoped_lock lock(handler_mutex); active_stop = nullptr;
    }
    if (!result) {
        std::wcerr << definition.display_name << L": " << result.Error().Message() << L'\n';
        return static_cast<int>(result.Error().code);
    }
    return 0;
}
int RunWindowsService(const ServiceDefinition& definition, ServiceMain main) {
    if (definition.name.empty() || !main) return ERROR_INVALID_PARAMETER;
    active_definition = &definition;
    active_main = &main;
    SERVICE_TABLE_ENTRYW table[] = {
        {const_cast<wchar_t*>(definition.name.c_str()), ServiceEntry}, {nullptr, nullptr}};
    const BOOL started = StartServiceCtrlDispatcherW(table);
    const DWORD error = started ? ERROR_SUCCESS : GetLastError();
    active_definition = nullptr;
    active_main = nullptr;
    return static_cast<int>(error);
}
}  // namespace mwfl
