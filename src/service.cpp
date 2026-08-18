#include <algorithm>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <mwfl/service.h>
#include <optional>
#include <thread>
namespace mwfl {
struct ServiceRuntimeState {
    std::stop_source stop;
    mutable std::mutex mutex;
    mutable std::condition_variable_any changed;
    bool paused = false;
};
struct ServiceContextAccess {
    static ServiceContext Make(std::shared_ptr<ServiceRuntimeState> state) {
        return ServiceContext(std::move(state));
    }
};
namespace {
std::mutex host_mutex;
std::shared_ptr<ServiceRuntimeState> active_state;
const ServiceDefinition* active_definition = nullptr;
ServiceCallbacks* active_callbacks = nullptr;
SERVICE_STATUS_HANDLE active_status_handle = nullptr;
SERVICE_STATUS active_status{};
DWORD NativeState(ServiceState state) {
    switch (state) {
        case ServiceState::StartPending:
            return SERVICE_START_PENDING;
        case ServiceState::Running:
            return SERVICE_RUNNING;
        case ServiceState::PausePending:
            return SERVICE_PAUSE_PENDING;
        case ServiceState::Paused:
            return SERVICE_PAUSED;
        case ServiceState::ContinuePending:
            return SERVICE_CONTINUE_PENDING;
        case ServiceState::StopPending:
            return SERVICE_STOP_PENDING;
        default:
            return SERVICE_STOPPED;
    }
}
void NotifyState(ServiceState state, DWORD win32 = NO_ERROR, DWORD specific = 0,
                 DWORD checkpoint = 0, DWORD hint = 0) noexcept {
    if (active_callbacks && active_callbacks->state_changed) {
        try {
            active_callbacks->state_changed(state);
        } catch (...) {
        }
    }
    if (!active_status_handle) return;
    active_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    active_status.dwCurrentState = NativeState(state);
    active_status.dwWin32ExitCode = specific ? ERROR_SERVICE_SPECIFIC_ERROR : win32;
    active_status.dwServiceSpecificExitCode = specific;
    active_status.dwCheckPoint = checkpoint;
    active_status.dwWaitHint = hint;
    active_status.dwControlsAccepted =
        state == ServiceState::Running
            ? SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN |
                  (active_definition && active_definition->supports_pause
                       ? SERVICE_ACCEPT_PAUSE_CONTINUE
                       : 0) |
                  SERVICE_ACCEPT_POWEREVENT | SERVICE_ACCEPT_SESSIONCHANGE
            : 0;
    SetServiceStatus(active_status_handle, &active_status);
}
BOOL WINAPI ConsoleHandler(DWORD control) noexcept {
    if (control != CTRL_C_EVENT && control != CTRL_BREAK_EVENT && control != CTRL_CLOSE_EVENT &&
        control != CTRL_SHUTDOWN_EVENT)
        return FALSE;
    std::shared_ptr<ServiceRuntimeState> state;
    {
        std::scoped_lock lock(host_mutex);
        state = active_state;
    }
    if (state) {
        state->stop.request_stop();
        state->changed.notify_all();
    }
    return TRUE;
}
DWORD WINAPI ServiceControl(DWORD control, DWORD event_type, void* event_data, void*) noexcept {
    try {
        std::shared_ptr<ServiceRuntimeState> state;
        ServiceCallbacks* callbacks = nullptr;
        {
            std::scoped_lock lock(host_mutex);
            state = active_state;
            callbacks = active_callbacks;
        }
        if (!state) return NO_ERROR;
        ServiceControlEvent event;
        event.native_control = control;
        event.event_type = event_type;
        event.event_data = event_data;
        switch (control) {
            case SERVICE_CONTROL_STOP:
                event.kind = ServiceControlKind::Stop;
                break;
            case SERVICE_CONTROL_SHUTDOWN:
                event.kind = ServiceControlKind::Shutdown;
                break;
            case SERVICE_CONTROL_PAUSE:
                event.kind = ServiceControlKind::Pause;
                break;
            case SERVICE_CONTROL_CONTINUE:
                event.kind = ServiceControlKind::Continue;
                break;
            case SERVICE_CONTROL_POWEREVENT:
                event.kind = ServiceControlKind::Power;
                break;
            case SERVICE_CONTROL_SESSIONCHANGE:
                event.kind = ServiceControlKind::Session;
                break;
            default:
                if (control >= 128)
                    event.kind = ServiceControlKind::Custom;
                else if (control == SERVICE_CONTROL_INTERROGATE) {
                    SetServiceStatus(active_status_handle, &active_status);
                    return NO_ERROR;
                } else
                    return ERROR_CALL_NOT_IMPLEMENTED;
        }
        auto context = ServiceContextAccess::Make(state);
        if (callbacks && callbacks->control) {
            auto handled = callbacks->control(context, event);
            if (!handled) return handled.Error().code;
        }
        if (event.kind == ServiceControlKind::Stop || event.kind == ServiceControlKind::Shutdown) {
            NotifyState(ServiceState::StopPending, NO_ERROR, 0, 1,
                        active_definition
                            ? static_cast<DWORD>(
                                  (std::min)(active_definition->stop_timeout.count(), 120000LL))
                            : 30000);
            state->stop.request_stop();
        } else if (event.kind == ServiceControlKind::Pause && active_definition &&
                   active_definition->supports_pause) {
            NotifyState(ServiceState::PausePending, NO_ERROR, 0, 1, 5000);
            {
                std::scoped_lock lock(state->mutex);
                state->paused = true;
            }
            NotifyState(ServiceState::Paused);
        } else if (event.kind == ServiceControlKind::Continue && active_definition &&
                   active_definition->supports_pause) {
            NotifyState(ServiceState::ContinuePending, NO_ERROR, 0, 1, 5000);
            {
                std::scoped_lock lock(state->mutex);
                state->paused = false;
            }
            state->changed.notify_all();
            NotifyState(ServiceState::Running);
        }
        state->changed.notify_all();
        return NO_ERROR;
    } catch (...) {
        return ERROR_EXCEPTION_IN_SERVICE;
    }
}
Result<ServiceExit> Invoke(ServiceCallbacks& callbacks, ServiceContext& context) {
    try {
        if (!callbacks.run) return NativeError::FromWin32(ERROR_INVALID_PARAMETER);
        return callbacks.run(context);
    } catch (...) {
        return NativeError{ErrorDomain::Application, ERROR_UNHANDLED_EXCEPTION,
                           L"Service run callback"};
    }
}
void WINAPI ServiceEntry(DWORD, wchar_t**) noexcept {
    ServiceExit exit{ERROR_EXCEPTION_IN_SERVICE, 0};
    try {
        active_status_handle =
            RegisterServiceCtrlHandlerExW(active_definition->name.c_str(), ServiceControl, nullptr);
        if (!active_status_handle) return;
        NotifyState(ServiceState::StartPending, NO_ERROR, 0, 1, 10000);
        auto state = std::make_shared<ServiceRuntimeState>();
        {
            std::scoped_lock lock(host_mutex);
            active_state = state;
        }
        auto context = ServiceContextAccess::Make(state);
        NotifyState(ServiceState::Running);
        Result<ServiceExit> result = NativeError::FromWin32(ERROR_GEN_FAILURE);
        std::jthread worker([&] { result = Invoke(*active_callbacks, context); });
        DWORD checkpoint = 1;
        std::optional<std::chrono::steady_clock::time_point> stop_started;
        bool stop_timed_out = false;
        while (worker.joinable()) {
            if (WaitForSingleObject(worker.native_handle(), 1000) == WAIT_OBJECT_0) {
                worker.join();
                break;
            }
            if (state->stop.stop_requested()) {
                if (!stop_started) stop_started = std::chrono::steady_clock::now();
                const auto allowed = active_definition ? active_definition->stop_timeout
                                                       : std::chrono::milliseconds(30000);
                if (!stop_timed_out &&
                    std::chrono::steady_clock::now() - *stop_started >= allowed) {
                    stop_timed_out = true;
                    NotifyState(ServiceState::StopPending, ERROR_SERVICE_REQUEST_TIMEOUT, 0,
                                ++checkpoint, 1000);
                } else {
                    NotifyState(ServiceState::StopPending,
                                stop_timed_out ? ERROR_SERVICE_REQUEST_TIMEOUT : NO_ERROR, 0,
                                ++checkpoint,
                                stop_timed_out ? 1000 : static_cast<DWORD>(allowed.count()));
                }
            }
        }
        if (result)
            exit = result.Value();
        else
            exit.win32_code = result.Error().code;
        if (stop_timed_out && exit.win32_code == ERROR_SUCCESS)
            exit.win32_code = ERROR_SERVICE_REQUEST_TIMEOUT;
        {
            std::scoped_lock lock(host_mutex);
            active_state.reset();
        }
    } catch (...) {
        exit.win32_code = ERROR_EXCEPTION_IN_SERVICE;
    }
    NotifyState(ServiceState::Stopped, exit.win32_code, exit.service_specific_code);
    active_status_handle = nullptr;
}
bool Allowed(ServiceState from, ServiceState to) noexcept {
    switch (from) {
        case ServiceState::Stopped:
            return to == ServiceState::StartPending;
        case ServiceState::StartPending:
            return to == ServiceState::Running || to == ServiceState::StopPending;
        case ServiceState::Running:
            return to == ServiceState::PausePending || to == ServiceState::StopPending;
        case ServiceState::PausePending:
            return to == ServiceState::Paused || to == ServiceState::StopPending;
        case ServiceState::Paused:
            return to == ServiceState::ContinuePending || to == ServiceState::StopPending;
        case ServiceState::ContinuePending:
            return to == ServiceState::Running || to == ServiceState::StopPending;
        case ServiceState::StopPending:
            return to == ServiceState::Stopped;
    }
    return false;
}
const wchar_t* AccountName(ServiceAccount account) {
    switch (account) {
        case ServiceAccount::LocalService:
            return L"NT AUTHORITY\\LocalService";
        case ServiceAccount::NetworkService:
            return L"NT AUTHORITY\\NetworkService";
        default:
            return L"LocalSystem";
    }
}
std::wstring QuoteArgument(std::wstring_view argument) {
    if (!argument.empty() && argument.find_first_of(L" \t\"") == std::wstring_view::npos)
        return std::wstring(argument);
    std::wstring quoted(1, L'"');
    std::size_t slashes = 0;
    for (wchar_t c : argument) {
        if (c == L'\\') {
            ++slashes;
            continue;
        }
        if (c == L'"') {
            quoted.append(slashes * 2 + 1, L'\\');
            quoted.push_back(L'"');
            slashes = 0;
            continue;
        }
        quoted.append(slashes, L'\\');
        slashes = 0;
        quoted.push_back(c);
    }
    quoted.append(slashes * 2, L'\\');
    quoted.push_back(L'"');
    return quoted;
}
std::wstring BinaryCommand(const ServiceInstallSpec& spec) {
    std::wstring value = QuoteArgument(spec.executable.wstring());
    for (const auto& arg : spec.arguments) value += L" " + QuoteArgument(arg);
    return value;
}
Result<ServiceSnapshot> Snapshot(SC_HANDLE service, std::wstring name) {
    DWORD bytes = 0;
    QueryServiceConfigW(service, nullptr, 0, &bytes);
    std::vector<std::byte> config_storage(bytes);
    auto* config = reinterpret_cast<QUERY_SERVICE_CONFIGW*>(config_storage.data());
    if (!QueryServiceConfigW(service, config, bytes, &bytes)) return NativeError::LastWin32();
    SERVICE_STATUS_PROCESS status{};
    if (!QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO, reinterpret_cast<BYTE*>(&status),
                              sizeof(status), &bytes))
        return NativeError::LastWin32();
    return ServiceSnapshot{std::move(name),
                           config->lpDisplayName ? config->lpDisplayName : L"",
                           config->lpBinaryPathName ? config->lpBinaryPathName : L"",
                           config->lpServiceStartName ? config->lpServiceStartName : L"",
                           config->dwStartType,
                           status.dwCurrentState,
                           status.dwProcessId,
                           status.dwWin32ExitCode,
                           status.dwServiceSpecificExitCode};
}
Result<ServiceOperationResult> WaitState(SC_HANDLE service, std::wstring name, DWORD desired,
                                         std::chrono::milliseconds timeout, std::stop_token stop) {
    const auto end = std::chrono::steady_clock::now() + timeout;
    while (true) {
        auto snapshot = Snapshot(service, name);
        if (!snapshot) return snapshot.Error();
        if (snapshot.Value().state == desired)
            return ServiceOperationResult{ServiceOperationStatus::ReachedTarget,
                                          std::move(snapshot.Value())};
        if (stop.stop_requested())
            return ServiceOperationResult{ServiceOperationStatus::Cancelled,
                                          std::move(snapshot.Value())};
        if (timeout.count() >= 0 && std::chrono::steady_clock::now() >= end)
            return ServiceOperationResult{ServiceOperationStatus::TimedOut,
                                          std::move(snapshot.Value())};
        Sleep(50);
    }
}
}  // namespace
bool ServiceStateMachine::Transition(ServiceState next) noexcept {
    if (!Allowed(state_, next)) return false;
    state_ = next;
    return true;
}
std::stop_token ServiceContext::StopToken() const noexcept {
    auto state = std::static_pointer_cast<ServiceRuntimeState>(state_);
    return state ? state->stop.get_token() : std::stop_token{};
}
bool ServiceContext::IsPaused() const noexcept {
    auto state = std::static_pointer_cast<ServiceRuntimeState>(state_);
    if (!state) return false;
    std::scoped_lock lock(state->mutex);
    return state->paused;
}
WaitStatus ServiceContext::WaitWhilePaused(std::chrono::milliseconds timeout) const {
    auto state = std::static_pointer_cast<ServiceRuntimeState>(state_);
    if (!state) return WaitStatus::Cancelled;
    std::unique_lock lock(state->mutex);
    if (!state->paused) return WaitStatus::Signaled;
    const auto predicate = [&] { return !state->paused || state->stop.stop_requested(); };
    const bool ready =
        timeout.count() < 0
            ? (state->changed.wait(lock, state->stop.get_token(), predicate), true)
            : state->changed.wait_for(lock, state->stop.get_token(), timeout, predicate);
    if (state->stop.stop_requested()) return WaitStatus::Cancelled;
    return ready ? WaitStatus::Signaled : WaitStatus::Timeout;
}
int RunServiceConsole(const ServiceDefinition& definition, ServiceCallbacks callbacks) {
    if (definition.name.empty() || !callbacks.run) return ERROR_INVALID_PARAMETER;
    auto state = std::make_shared<ServiceRuntimeState>();
    {
        std::scoped_lock lock(host_mutex);
        active_state = state;
        active_definition = &definition;
        active_callbacks = &callbacks;
    }
    if (!SetConsoleCtrlHandler(ConsoleHandler, TRUE)) return static_cast<int>(GetLastError());
    auto context = ServiceContextAccess::Make(state);
    if (callbacks.state_changed) callbacks.state_changed(ServiceState::Running);
    auto result = Invoke(callbacks, context);
    SetConsoleCtrlHandler(ConsoleHandler, FALSE);
    {
        std::scoped_lock lock(host_mutex);
        active_state.reset();
        active_definition = nullptr;
        active_callbacks = nullptr;
    }
    if (!result) {
        std::wcerr << definition.display_name << L": " << result.Error().Message() << L'\n';
        return result.Error().code;
    }
    return result.Value().service_specific_code ? ERROR_SERVICE_SPECIFIC_ERROR
                                                : result.Value().win32_code;
}
int RunWindowsService(const ServiceDefinition& definition, ServiceCallbacks callbacks) {
    if (definition.name.empty() || !callbacks.run) return ERROR_INVALID_PARAMETER;
    {
        std::scoped_lock lock(host_mutex);
        active_definition = &definition;
        active_callbacks = &callbacks;
    }
    SERVICE_TABLE_ENTRYW table[]{{const_cast<wchar_t*>(definition.name.c_str()), ServiceEntry},
                                 {nullptr, nullptr}};
    const BOOL started = StartServiceCtrlDispatcherW(table);
    const DWORD error = started ? ERROR_SUCCESS : GetLastError();
    {
        std::scoped_lock lock(host_mutex);
        active_definition = nullptr;
        active_callbacks = nullptr;
    }
    return error;
}
int RunServiceConsole(const ServiceDefinition& definition, ServiceMain main) {
    return RunServiceConsole(
        definition, ServiceCallbacks{[main = std::move(main)](
                                         ServiceContext& context) mutable -> Result<ServiceExit> {
            auto result = main(context.StopToken());
            return result ? Result<ServiceExit>{ServiceExit{}}
                          : Result<ServiceExit>{result.Error()};
        }});
}
int RunWindowsService(const ServiceDefinition& definition, ServiceMain main) {
    return RunWindowsService(
        definition, ServiceCallbacks{[main = std::move(main)](
                                         ServiceContext& context) mutable -> Result<ServiceExit> {
            auto result = main(context.StopToken());
            return result ? Result<ServiceExit>{ServiceExit{}}
                          : Result<ServiceExit>{result.Error()};
        }});
}
ServiceManager::~ServiceManager() {
    if (manager_) CloseServiceHandle(manager_);
}
ServiceManager::ServiceManager(ServiceManager&& other) noexcept
    : manager_(std::exchange(other.manager_, nullptr)), access_(other.access_) {}
ServiceManager& ServiceManager::operator=(ServiceManager&& other) noexcept {
    if (this != &other) {
        if (manager_) CloseServiceHandle(manager_);
        manager_ = std::exchange(other.manager_, nullptr);
        access_ = other.access_;
    }
    return *this;
}
Result<ServiceManager> ServiceManager::Open(Access access) {
    const DWORD desired =
        SC_MANAGER_CONNECT | (access == Access::Manage ? SC_MANAGER_CREATE_SERVICE : 0);
    SC_HANDLE handle = OpenSCManagerW(nullptr, nullptr, desired);
    if (!handle) return NativeError::LastWin32().WithOperation(L"OpenSCManagerW");
    return ServiceManager(handle, access);
}
Result<ServiceQueryResult> ServiceManager::Query(std::wstring_view name) const {
    std::wstring copy(name);
    SC_HANDLE raw =
        OpenServiceW(manager_, copy.c_str(), SERVICE_QUERY_CONFIG | SERVICE_QUERY_STATUS);
    if (!raw) {
        if (GetLastError() == ERROR_SERVICE_DOES_NOT_EXIST) return ServiceQueryResult{};
        return NativeError::LastWin32();
    }
    auto snapshot = Snapshot(raw, copy);
    CloseServiceHandle(raw);
    return snapshot ? ServiceQueryResult{ServiceQueryStatus::Found, std::move(snapshot.Value())}
                    : Result<ServiceQueryResult>{snapshot.Error()};
}
Result<ServiceMutationResult> ServiceManager::InstallOrUpdate(
    const ServiceInstallSpec& spec) const {
    if (access_ != Access::Manage)
        return NativeError::FromWin32(ERROR_ACCESS_DENIED)
            .WithOperation(L"ServiceManager opened for query");
    if (spec.name.empty() || spec.executable.empty())
        return NativeError::FromWin32(ERROR_INVALID_PARAMETER);
    const std::wstring command = BinaryCommand(spec);
    SC_HANDLE service =
        OpenServiceW(manager_, spec.name.c_str(),
                     SERVICE_CHANGE_CONFIG | SERVICE_QUERY_CONFIG | SERVICE_QUERY_STATUS);
    bool created = false;
    if (!service && GetLastError() == ERROR_SERVICE_DOES_NOT_EXIST) {
        service = CreateServiceW(
            manager_, spec.name.c_str(), spec.display_name.c_str(),
            SERVICE_CHANGE_CONFIG | SERVICE_QUERY_CONFIG | SERVICE_QUERY_STATUS,
            SERVICE_WIN32_OWN_PROCESS, spec.start_type, SERVICE_ERROR_NORMAL, command.c_str(),
            nullptr, nullptr, nullptr, AccountName(spec.account), nullptr);
        created = true;
    }
    if (!service) return NativeError::LastWin32();
    bool changed = created;
    std::vector<std::wstring> fields;
    if (!created) {
        auto before = Snapshot(service, spec.name);
        if (!before) {
            CloseServiceHandle(service);
            return before.Error();
        }
        if (before.Value().display_name != spec.display_name) fields.push_back(L"display_name");
        if (before.Value().binary_path != command) fields.push_back(L"binary_path");
        if (before.Value().start_type != spec.start_type) fields.push_back(L"start_type");
        if (_wcsicmp(before.Value().account_name.c_str(), AccountName(spec.account)) != 0)
            fields.push_back(L"account");
        changed = !fields.empty();
        if (changed &&
            !ChangeServiceConfigW(service, SERVICE_NO_CHANGE, spec.start_type, SERVICE_NO_CHANGE,
                                  command.c_str(), nullptr, nullptr, nullptr,
                                  AccountName(spec.account), nullptr, spec.display_name.c_str())) {
            const auto error = NativeError::LastWin32();
            CloseServiceHandle(service);
            return error;
        }
    }
    auto after = Snapshot(service, spec.name);
    CloseServiceHandle(service);
    if (!after) return after.Error();
    return ServiceMutationResult{created, changed, std::move(fields), std::move(after.Value())};
}
Result<ServiceOperationResult> ServiceManager::Start(std::wstring_view name,
                                                     std::chrono::milliseconds timeout,
                                                     std::stop_token stop) const {
    std::wstring copy(name);
    SC_HANDLE service = OpenServiceW(manager_, copy.c_str(),
                                     SERVICE_START | SERVICE_QUERY_STATUS | SERVICE_QUERY_CONFIG);
    if (!service) return NativeError::LastWin32();
    if (!StartServiceW(service, 0, nullptr) && GetLastError() != ERROR_SERVICE_ALREADY_RUNNING) {
        auto error = NativeError::LastWin32();
        CloseServiceHandle(service);
        return error;
    }
    auto result = WaitState(service, copy, SERVICE_RUNNING, timeout, stop);
    CloseServiceHandle(service);
    return result;
}
Result<ServiceOperationResult> ServiceManager::Stop(std::wstring_view name,
                                                    std::chrono::milliseconds timeout,
                                                    std::stop_token stop) const {
    std::wstring copy(name);
    SC_HANDLE service = OpenServiceW(manager_, copy.c_str(),
                                     SERVICE_STOP | SERVICE_QUERY_STATUS | SERVICE_QUERY_CONFIG);
    if (!service) return NativeError::LastWin32();
    SERVICE_STATUS status{};
    if (!ControlService(service, SERVICE_CONTROL_STOP, &status) &&
        GetLastError() != ERROR_SERVICE_NOT_ACTIVE) {
        auto error = NativeError::LastWin32();
        CloseServiceHandle(service);
        return error;
    }
    auto result = WaitState(service, copy, SERVICE_STOPPED, timeout, stop);
    CloseServiceHandle(service);
    return result;
}
Result<ServiceSnapshot> ServiceManager::SendControl(std::wstring_view name, DWORD control) const {
    std::wstring copy(name);
    SC_HANDLE service = OpenServiceW(manager_, copy.c_str(),
                                     SERVICE_USER_DEFINED_CONTROL | SERVICE_PAUSE_CONTINUE |
                                         SERVICE_QUERY_STATUS | SERVICE_QUERY_CONFIG);
    if (!service) return NativeError::LastWin32();
    SERVICE_STATUS status{};
    if (!ControlService(service, control, &status)) {
        auto error = NativeError::LastWin32();
        CloseServiceHandle(service);
        return error;
    }
    auto result = Snapshot(service, copy);
    CloseServiceHandle(service);
    return result;
}
Result<bool> ServiceManager::Remove(std::wstring_view name) const {
    std::wstring copy(name);
    SC_HANDLE service = OpenServiceW(manager_, copy.c_str(), DELETE);
    if (!service) {
        if (GetLastError() == ERROR_SERVICE_DOES_NOT_EXIST) return false;
        return NativeError::LastWin32();
    }
    const BOOL removed = DeleteService(service);
    const DWORD error = removed ? ERROR_SUCCESS : GetLastError();
    CloseServiceHandle(service);
    if (!removed && error != ERROR_SERVICE_MARKED_FOR_DELETE) return NativeError::FromWin32(error);
    return true;
}
}  // namespace mwfl
