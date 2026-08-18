#include <iomanip>
#include <mwfl/scheduler.h>
#include <sstream>
#include <taskschd.h>
#include <wrl/client.h>
namespace mwfl {
namespace {
using Microsoft::WRL::ComPtr;
class ComScope {
   public:
    ComScope() {
        result_ = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        owns_ = result_ == S_OK || result_ == S_FALSE;
    }
    ~ComScope() {
        if (owns_) CoUninitialize();
    }
    bool Usable() const { return SUCCEEDED(result_) || result_ == RPC_E_CHANGED_MODE; }
    HRESULT Result() const { return result_; }

   private:
    HRESULT result_ = E_FAIL;
    bool owns_ = false;
};
VARIANT EmptyVariant() {
    VARIANT value;
    VariantInit(&value);
    return value;
}
std::wstring Quote(std::wstring_view value) {
    if (!value.empty() && value.find_first_of(L" \t\"") == std::wstring_view::npos)
        return std::wstring(value);
    std::wstring result(1, L'"');
    std::size_t slashes = 0;
    for (wchar_t c : value) {
        if (c == L'\\') {
            ++slashes;
            continue;
        }
        if (c == L'"') {
            result.append(slashes * 2 + 1, L'\\');
            result.push_back(L'"');
            slashes = 0;
            continue;
        }
        result.append(slashes, L'\\');
        slashes = 0;
        result.push_back(c);
    }
    result.append(slashes * 2, L'\\');
    result.push_back(L'"');
    return result;
}
std::wstring Arguments(const std::vector<std::wstring>& values) {
    std::wstring result;
    for (const auto& value : values) {
        if (!result.empty()) result.push_back(L' ');
        result += Quote(value);
    }
    return result;
}
std::wstring Boundary(std::chrono::system_clock::time_point time) {
    const std::time_t raw = std::chrono::system_clock::to_time_t(time);
    std::tm utc{};
    gmtime_s(&utc, &raw);
    std::wostringstream text;
    text << std::put_time(&utc, L"%Y-%m-%dT%H:%M:%SZ");
    return text.str();
}
Result<ComPtr<ITaskService>> Connect() {
    ComPtr<ITaskService> service;
    const HRESULT created = CoCreateInstance(CLSID_TaskScheduler, nullptr, CLSCTX_INPROC_SERVER,
                                             IID_PPV_ARGS(&service));
    if (FAILED(created))
        return SystemError::FromHResult(created).WithOperation(L"CoCreateInstance TaskScheduler");
    VARIANT empty = EmptyVariant();
    const HRESULT connected = service->Connect(empty, empty, empty, empty);
    if (FAILED(connected))
        return SystemError::FromHResult(connected).WithOperation(L"ITaskService::Connect");
    return service;
}
Result<ComPtr<ITaskFolder>> OpenFolder(ITaskService* service, std::wstring_view folder,
                                       bool create) {
    BSTR path = SysAllocString(std::wstring(folder).c_str());
    ComPtr<ITaskFolder> result;
    HRESULT hr = service->GetFolder(path, &result);
    SysFreeString(path);
    if (SUCCEEDED(hr) || !create)
        return SUCCEEDED(hr) ? Result<ComPtr<ITaskFolder>>{result}
                             : Result<ComPtr<ITaskFolder>>{SystemError::FromHResult(hr)};
    ComPtr<ITaskFolder> root;
    BSTR root_name = SysAllocString(L"\\");
    hr = service->GetFolder(root_name, &root);
    SysFreeString(root_name);
    if (FAILED(hr)) return SystemError::FromHResult(hr);
    std::wstring relative(folder);
    if (!relative.empty() && relative.front() == L'\\') relative.erase(relative.begin());
    BSTR name = SysAllocString(relative.c_str());
    VARIANT sddl = EmptyVariant();
    hr = root->CreateFolder(name, sddl, &result);
    SysFreeString(name);
    if (FAILED(hr))
        return SystemError::FromHResult(hr).WithOperation(L"Create Task Scheduler folder");
    return result;
}
std::wstring TakeBstr(BSTR value) {
    std::wstring result = value ? value : L"";
    SysFreeString(value);
    return result;
}
Result<ScheduledTaskSnapshot> Snapshot(ITaskFolder* folder, std::wstring_view folder_name,
                                       std::wstring_view task_name) {
    BSTR name = SysAllocString(std::wstring(task_name).c_str());
    ComPtr<IRegisteredTask> task;
    const HRESULT hr = folder->GetTask(name, &task);
    SysFreeString(name);
    if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) return ScheduledTaskSnapshot{};
    if (FAILED(hr)) return SystemError::FromHResult(hr);
    ScheduledTaskSnapshot result;
    result.exists = true;
    result.folder = folder_name;
    result.name = task_name;
    TASK_STATE state{};
    VARIANT_BOOL enabled = VARIANT_FALSE;
    task->get_State(&state);
    task->get_Enabled(&enabled);
    result.state = static_cast<LONG>(state);
    result.enabled = enabled == VARIANT_TRUE;
    ComPtr<ITaskDefinition> definition;
    if (FAILED(task->get_Definition(&definition))) return SystemError::FromHResult(E_FAIL);
    ComPtr<IRegistrationInfo> registration;
    definition->get_RegistrationInfo(&registration);
    BSTR text = nullptr;
    if (registration && SUCCEEDED(registration->get_Description(&text)))
        result.description = TakeBstr(text);
    ComPtr<IActionCollection> actions;
    definition->get_Actions(&actions);
    LONG action_count = 0;
    if (actions) actions->get_Count(&action_count);
    if (action_count == 1) {
        ComPtr<IAction> action;
        if (SUCCEEDED(actions->get_Item(1, &action))) {
            TASK_ACTION_TYPE type{};
            action->get_Type(&type);
            if (type == TASK_ACTION_EXEC) {
                ComPtr<IExecAction> exec;
                action.As(&exec);
                if (exec && SUCCEEDED(exec->get_Path(&text))) result.executable = TakeBstr(text);
                text = nullptr;
                if (exec && SUCCEEDED(exec->get_Arguments(&text)))
                    result.arguments = TakeBstr(text);
            }
        }
    }
    ComPtr<ITriggerCollection> triggers;
    definition->get_Triggers(&triggers);
    LONG trigger_count = 0;
    if (triggers) triggers->get_Count(&trigger_count);
    if (trigger_count == 1) {
        ComPtr<ITrigger> trigger;
        if (SUCCEEDED(triggers->get_Item(1, &trigger))) {
            TASK_TRIGGER_TYPE2 type{};
            trigger->get_Type(&type);
            result.trigger = type == TASK_TRIGGER_LOGON ? ScheduledTaskTrigger::CurrentUserLogon
                                                        : ScheduledTaskTrigger::Once;
            if (type == TASK_TRIGGER_TIME && SUCCEEDED(trigger->get_StartBoundary(&text)))
                result.start_boundary = TakeBstr(text);
        }
    }
    return result;
}
bool Equivalent(const ScheduledTaskSnapshot& current, const TaskDefinition& desired) {
    if (!current.exists || !current.enabled || current.description != desired.description ||
        current.executable != desired.executable ||
        current.arguments != Arguments(desired.arguments) || current.trigger != desired.trigger)
        return false;
    return desired.trigger != ScheduledTaskTrigger::Once ||
           current.start_boundary == Boundary(desired.start_time);
}
}  // namespace
Result<void> ValidateScheduledTask(const TaskDefinition& spec) {
    if (spec.folder.empty() || spec.folder == L"\\" || spec.folder.front() != L'\\' ||
        spec.folder.find(L'/') != std::wstring::npos ||
        spec.folder.find(L"..") != std::wstring::npos ||
        spec.name.empty() || spec.name.find_first_of(L"\\/") != std::wstring::npos ||
        spec.executable.empty())
        return SystemError::FromWin32(ERROR_INVALID_PARAMETER)
            .WithOperation(L"Scheduled task definition");
    if (spec.trigger == ScheduledTaskTrigger::Once &&
        spec.start_time == std::chrono::system_clock::time_point{})
        return SystemError::FromWin32(ERROR_INVALID_TIME);
    return {};
}
Result<ScheduledTaskSnapshot> TaskScheduler::Query(std::wstring_view folder,
                                                   std::wstring_view name) const {
    ComScope com;
    if (!com.Usable()) return SystemError::FromHResult(com.Result());
    auto service = Connect();
    if (!service) return service.GetError();
    auto opened = OpenFolder(service.Value().Get(), folder, false);
    if (!opened) {
        if (opened.GetError().code == static_cast<DWORD>(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)))
            return ScheduledTaskSnapshot{};
        return opened.GetError();
    }
    return Snapshot(opened.Value().Get(), folder, name);
}
Result<TaskChangePlan> TaskScheduler::Plan(const TaskDefinition& spec) const {
    auto valid = ValidateScheduledTask(spec);
    if (!valid) return valid.GetError();
    auto before = Query(spec.folder, spec.name);
    if (!before) return before.GetError();
    const bool equivalent = Equivalent(before.Value(), spec);
    const bool creates = !before.Value().exists;
    return TaskChangePlan{spec, std::move(before.Value()), !equivalent, creates};
}
Result<TaskApplyResult> TaskScheduler::Apply(const TaskChangePlan& plan) const {
    auto replanned = Plan(plan.desired);
    if (!replanned) return replanned.GetError();
    const auto& spec = replanned.Value().desired;
    auto before = replanned.Value().current;
    if (!replanned.Value().required)
        return TaskApplyResult{false, false, std::move(before)};
    ComScope com;
    if (!com.Usable()) return SystemError::FromHResult(com.Result());
    auto service = Connect();
    if (!service) return service.GetError();
    auto folder = OpenFolder(service.Value().Get(), spec.folder, true);
    if (!folder) return folder.GetError();
    ComPtr<ITaskDefinition> definition;
    HRESULT hr = service.Value()->NewTask(0, &definition);
    if (FAILED(hr)) return SystemError::FromHResult(hr);
    ComPtr<IRegistrationInfo> registration;
    definition->get_RegistrationInfo(&registration);
    BSTR description = SysAllocString(spec.description.c_str());
    registration->put_Description(description);
    SysFreeString(description);
    ComPtr<ITaskSettings> settings;
    definition->get_Settings(&settings);
    settings->put_Enabled(VARIANT_TRUE);
    settings->put_StartWhenAvailable(VARIANT_TRUE);
    ComPtr<ITriggerCollection> triggers;
    definition->get_Triggers(&triggers);
    if (spec.trigger != ScheduledTaskTrigger::OnDemand) {
        ComPtr<ITrigger> trigger;
        hr = triggers->Create(spec.trigger == ScheduledTaskTrigger::CurrentUserLogon
                                  ? TASK_TRIGGER_LOGON
                                  : TASK_TRIGGER_TIME,
                              &trigger);
        if (FAILED(hr)) return SystemError::FromHResult(hr);
        if (spec.trigger == ScheduledTaskTrigger::Once) {
            ComPtr<ITimeTrigger> timed;
            trigger.As(&timed);
            BSTR boundary = SysAllocString(Boundary(spec.start_time).c_str());
            timed->put_StartBoundary(boundary);
            SysFreeString(boundary);
        }
    }
    ComPtr<IActionCollection> actions;
    definition->get_Actions(&actions);
    ComPtr<IAction> action;
    hr = actions->Create(TASK_ACTION_EXEC, &action);
    if (FAILED(hr)) return SystemError::FromHResult(hr);
    ComPtr<IExecAction> exec;
    action.As(&exec);
    BSTR path = SysAllocString(spec.executable.c_str());
    BSTR args = SysAllocString(Arguments(spec.arguments).c_str());
    exec->put_Path(path);
    exec->put_Arguments(args);
    SysFreeString(path);
    SysFreeString(args);
    VARIANT empty = EmptyVariant();
    BSTR task_name = SysAllocString(spec.name.c_str());
    ComPtr<IRegisteredTask> registered;
    hr = folder.Value()->RegisterTaskDefinition(task_name, definition.Get(), TASK_CREATE_OR_UPDATE,
                                                empty, empty, TASK_LOGON_INTERACTIVE_TOKEN, empty,
                                                &registered);
    SysFreeString(task_name);
    if (FAILED(hr)) return SystemError::FromHResult(hr).WithOperation(L"RegisterTaskDefinition");
    auto after = Snapshot(folder.Value().Get(), spec.folder, spec.name);
    if (!after) return after.GetError();
    return TaskApplyResult{!before.exists, true, std::move(after.Value())};
}
Result<void> TaskScheduler::Run(std::wstring_view folder, std::wstring_view name) const {
    ComScope com;
    if (!com.Usable()) return SystemError::FromHResult(com.Result());
    auto service = Connect();
    if (!service) return service.GetError();
    auto opened = OpenFolder(service.Value().Get(), folder, false);
    if (!opened) return opened.GetError();
    BSTR task_name = SysAllocString(std::wstring(name).c_str());
    ComPtr<IRegisteredTask> task;
    HRESULT hr = opened.Value()->GetTask(task_name, &task);
    SysFreeString(task_name);
    if (FAILED(hr)) return SystemError::FromHResult(hr);
    VARIANT empty = EmptyVariant();
    ComPtr<IRunningTask> running;
    hr = task->Run(empty, &running);
    return FAILED(hr) ? Result<void>{SystemError::FromHResult(hr)} : Result<void>{};
}
Result<void> TaskScheduler::Stop(std::wstring_view folder, std::wstring_view name) const {
    ComScope com;
    if (!com.Usable()) return SystemError::FromHResult(com.Result());
    auto service = Connect();
    if (!service) return service.GetError();
    auto opened = OpenFolder(service.Value().Get(), folder, false);
    if (!opened) return opened.GetError();
    BSTR task_name = SysAllocString(std::wstring(name).c_str());
    ComPtr<IRegisteredTask> task;
    HRESULT hr = opened.Value()->GetTask(task_name, &task);
    SysFreeString(task_name);
    if (FAILED(hr)) return SystemError::FromHResult(hr);
    hr = task->Stop(0);
    return FAILED(hr) ? Result<void>{SystemError::FromHResult(hr)} : Result<void>{};
}
Result<bool> TaskScheduler::Remove(std::wstring_view folder, std::wstring_view name) const {
    auto current = Query(folder, name);
    if (!current) return current.GetError();
    if (!current.Value().exists) return false;
    ComScope com;
    if (!com.Usable()) return SystemError::FromHResult(com.Result());
    auto service = Connect();
    if (!service) return service.GetError();
    auto opened = OpenFolder(service.Value().Get(), folder, false);
    if (!opened) return opened.GetError();
    BSTR task_name = SysAllocString(std::wstring(name).c_str());
    const HRESULT hr = opened.Value()->DeleteTask(task_name, 0);
    SysFreeString(task_name);
    if (FAILED(hr)) return SystemError::FromHResult(hr);
    return true;
}
}  // namespace mwfl
