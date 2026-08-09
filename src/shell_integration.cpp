#include <mwtl/shell_integration.h>

#include <propkey.h>
#include <propvarutil.h>
#include <shlobj_core.h>
#include <wil/resource.h>

#include <algorithm>
#include <cwctype>
#include <limits>
#include <utility>

namespace mwtl {
namespace {

ShellStatus MapError(HRESULT error) noexcept {
    if (SUCCEEDED(error)) return ShellStatus::success;
    if (error == REGDB_E_CLASSNOTREG || error == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
        return ShellStatus::unavailable;
    if (error == CO_E_NOTINITIALIZED || error == RPC_E_CHANGED_MODE)
        return ShellStatus::wrong_apartment;
    if (error == RPC_E_WRONG_THREAD) return ShellStatus::wrong_thread;
    if (error == E_INVALIDARG || error == E_POINTER) return ShellStatus::invalid_argument;
    if (error == E_ACCESSDENIED) return ShellStatus::rejected;
    return ShellStatus::com_failure;
}

bool Clean(std::wstring_view value) noexcept {
    return !value.empty() && value.find(L'\0') == std::wstring_view::npos;
}

bool IsSta() noexcept {
    APTTYPE type{};
    APTTYPEQUALIFIER qualifier{};
    return SUCCEEDED(::CoGetApartmentType(&type, &qualifier)) &&
           (type == APTTYPE_STA || type == APTTYPE_MAINSTA);
}

bool ValidTaskId(std::wstring_view value) noexcept {
    return Clean(value) && value.size() <= 64 &&
           std::all_of(value.begin(), value.end(), [](wchar_t ch) {
               return std::iswalnum(ch) || ch == L'-' || ch == L'_';
           });
}

TBPFLAG NativeState(TaskbarProgressState state) noexcept {
    switch (state) {
        case TaskbarProgressState::none: return TBPF_NOPROGRESS;
        case TaskbarProgressState::indeterminate: return TBPF_INDETERMINATE;
        case TaskbarProgressState::normal: return TBPF_NORMAL;
        case TaskbarProgressState::error: return TBPF_ERROR;
        case TaskbarProgressState::paused: return TBPF_PAUSED;
    }
    return TBPF_NOPROGRESS;
}

std::wstring TaskIdentity(std::wstring_view id) {
    return L"--mwtl-jump-task=" + std::wstring(id);
}

std::wstring ExtractTaskId(std::wstring_view arguments) {
    constexpr std::wstring_view prefix = L"--mwtl-jump-task=";
    if (!arguments.starts_with(prefix)) return {};
    const std::size_t end = arguments.find(L' ', prefix.size());
    return std::wstring(arguments.substr(prefix.size(), end - prefix.size()));
}

std::vector<std::wstring> ReadRemovedIds(IObjectArray* removed) {
    std::vector<std::wstring> ids;
    if (!removed) return ids;
    UINT count = 0;
    if (FAILED(removed->GetCount(&count))) return ids;
    for (UINT index = 0; index < count; ++index) {
        Microsoft::WRL::ComPtr<IShellLinkW> link;
        if (FAILED(removed->GetAt(index, IID_PPV_ARGS(&link)))) continue;
        wchar_t arguments[2048]{};
        if (SUCCEEDED(link->GetArguments(arguments, static_cast<int>(std::size(arguments))))) {
            const auto id = ExtractTaskId(arguments);
            if (!id.empty()) ids.push_back(id);
        }
    }
    return ids;
}

HRESULT MakeTaskLink(const std::filesystem::path& executable,
                     const JumpListTask& task, IShellLinkW** output) noexcept {
    if (!output) return E_POINTER;
    *output = nullptr;
    Microsoft::WRL::ComPtr<IShellLinkW> link;
    HRESULT result = ::CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                                         IID_PPV_ARGS(&link));
    if (FAILED(result)) return result;
    const std::wstring executable_string = executable.native();
    std::wstring arguments = TaskIdentity(task.id);
    if (!task.arguments.empty()) arguments += L" " + task.arguments;
    result = link->SetPath(executable_string.c_str());
    if (SUCCEEDED(result)) result = link->SetArguments(arguments.c_str());
    if (SUCCEEDED(result) && !task.icon.empty()) {
        const std::wstring icon = task.icon.native();
        result = link->SetIconLocation(icon.c_str(), task.icon_index);
    }
    Microsoft::WRL::ComPtr<IPropertyStore> properties;
    if (SUCCEEDED(result)) result = link.As(&properties);
    PROPVARIANT title{};
    if (SUCCEEDED(result)) result = ::InitPropVariantFromString(task.title.c_str(), &title);
    if (SUCCEEDED(result)) result = properties->SetValue(PKEY_Title, title);
    ::PropVariantClear(&title);
    if (SUCCEEDED(result)) result = properties->Commit();
    if (SUCCEEDED(result)) *output = link.Detach();
    return result;
}

}  // namespace

bool TaskbarProgressModel::SetState(TaskbarProgressState value) noexcept {
    state = value;
    if (value == TaskbarProgressState::none || value == TaskbarProgressState::indeterminate) {
        completed = 0;
        total = 0;
    }
    return IsValid();
}

bool TaskbarProgressModel::SetValue(std::uint64_t value, std::uint64_t maximum) noexcept {
    if (maximum == 0 || value > maximum) return false;
    completed = value;
    total = maximum;
    if (state == TaskbarProgressState::none || state == TaskbarProgressState::indeterminate)
        state = TaskbarProgressState::normal;
    return true;
}

void TaskbarProgressModel::Reset() noexcept { *this = {}; }

bool TaskbarProgressModel::IsValid() const noexcept {
    if (state == TaskbarProgressState::none || state == TaskbarProgressState::indeterminate)
        return completed == 0 && total == 0;
    return total > 0 && completed <= total;
}

ShellResult TaskbarWindowIntegration::CheckThread() const noexcept {
    if (!window_ || !::IsWindow(window_))
        return {ShellStatus::invalid_argument, E_HANDLE};
    if (::GetCurrentThreadId() != thread_id_)
        return {ShellStatus::wrong_thread, RPC_E_WRONG_THREAD};
    return {ShellStatus::success, S_OK};
}

ShellResult TaskbarWindowIntegration::Create(HWND window) noexcept {
    Reset();
    if (!window || !::IsWindow(window)) return {ShellStatus::invalid_argument, E_INVALIDARG};
    if (::GetWindowThreadProcessId(window, nullptr) != ::GetCurrentThreadId())
        return {ShellStatus::wrong_thread, RPC_E_WRONG_THREAD};
    window_ = window;
    thread_id_ = ::GetCurrentThreadId();
    HRESULT result = ::CoCreateInstance(CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER,
                                         IID_PPV_ARGS(&taskbar_));
    if (SUCCEEDED(result)) result = taskbar_->HrInit();
    if (FAILED(result)) {
        taskbar_.Reset();
        return {MapError(result), result};
    }
    return {ShellStatus::success, S_OK};
}

ShellResult TaskbarWindowIntegration::Recreate() noexcept {
    const auto check = CheckThread();
    if (!check) return check;
    const HWND window = window_;
    taskbar_.Reset();
    return Create(window);
}

ShellResult TaskbarWindowIntegration::Apply(const TaskbarProgressModel& progress) noexcept {
    const auto check = CheckThread();
    if (!check) return check;
    if (!taskbar_) return {ShellStatus::unavailable, E_NOINTERFACE};
    if (!progress.IsValid()) return {ShellStatus::invalid_argument, E_INVALIDARG};
    HRESULT result = taskbar_->SetProgressState(window_, NativeState(progress.state));
    if (SUCCEEDED(result) && progress.total > 0)
        result = taskbar_->SetProgressValue(window_, progress.completed, progress.total);
    return {MapError(SUCCEEDED(result) ? S_OK : result), result};
}

ShellResult TaskbarWindowIntegration::SetOverlayIcon(
    HICON icon, std::wstring_view accessible_description) noexcept {
    const auto check = CheckThread();
    if (!check) return check;
    if (!taskbar_) return {ShellStatus::unavailable, E_NOINTERFACE};
    if (accessible_description.find(L'\0') != std::wstring_view::npos)
        return {ShellStatus::invalid_argument, E_INVALIDARG};
    const std::wstring description(accessible_description);
    const HRESULT result = taskbar_->SetOverlayIcon(window_, icon, description.c_str());
    return {MapError(SUCCEEDED(result) ? S_OK : result), result};
}

ShellResult TaskbarWindowIntegration::Clear() noexcept {
    TaskbarProgressModel none;
    auto result = Apply(none);
    if (!result) return result;
    return SetOverlayIcon(nullptr, L"");
}

void TaskbarWindowIntegration::Reset() noexcept {
    taskbar_.Reset();
    window_ = nullptr;
    thread_id_ = 0;
}

UINT TaskbarWindowIntegration::GetTaskbarCreatedMessage() noexcept {
    static const UINT message = ::RegisterWindowMessageW(L"TaskbarCreated");
    return message;
}

JumpListPlan BuildJumpListPlan(std::span<const JumpListTask> tasks,
                               std::span<const std::wstring> removed_ids,
                               std::size_t maximum_slots) {
    JumpListPlan plan;
    plan.maximum_slots = maximum_slots;
    if (maximum_slots == 0) return plan;
    for (const auto& task : tasks) {
        if (!ValidTaskId(task.id) || !Clean(task.title) ||
            task.arguments.find(L'\0') != std::wstring::npos ||
            (!task.icon.empty() && !task.icon.is_absolute()))
            return {};
        if (std::any_of(plan.tasks.begin(), plan.tasks.end(),
                        [&](const JumpListTask& item) { return item.id == task.id; }))
            return {};
        if (std::find(removed_ids.begin(), removed_ids.end(), task.id) != removed_ids.end())
            continue;
        if (plan.tasks.size() < maximum_slots) plan.tasks.push_back(task);
    }
    plan.valid = true;
    return plan;
}

ShellResult CommitJumpList(const JumpListCommitOptions& options) noexcept {
    try {
        if (!Clean(options.app_id) || options.executable.empty() ||
            !options.executable.is_absolute() || options.tasks.empty())
            return {ShellStatus::invalid_argument, E_INVALIDARG};
        if (!IsSta()) return {ShellStatus::wrong_apartment, CO_E_NOTINITIALIZED};
        Microsoft::WRL::ComPtr<ICustomDestinationList> destination;
        HRESULT result = ::CoCreateInstance(CLSID_DestinationList, nullptr,
                                             CLSCTX_INPROC_SERVER,
                                             IID_PPV_ARGS(&destination));
        if (FAILED(result)) return {MapError(result), result};
        result = destination->SetAppID(options.app_id.c_str());
        UINT maximum_slots = 0;
        Microsoft::WRL::ComPtr<IObjectArray> removed;
        if (SUCCEEDED(result)) result = destination->BeginList(&maximum_slots, IID_PPV_ARGS(&removed));
        if (FAILED(result)) return {MapError(result), result};
        bool active = true;
        auto abort = wil::scope_exit([&] { if (active) destination->AbortList(); });
        const auto removed_ids = ReadRemovedIds(removed.Get());
        const auto plan = BuildJumpListPlan(options.tasks, removed_ids, maximum_slots);
        if (!plan.valid) return {ShellStatus::invalid_argument, E_INVALIDARG};
        Microsoft::WRL::ComPtr<IObjectCollection> collection;
        result = ::CoCreateInstance(CLSID_EnumerableObjectCollection, nullptr,
                                    CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&collection));
        for (const auto& task : plan.tasks) {
            Microsoft::WRL::ComPtr<IShellLinkW> link;
            if (SUCCEEDED(result)) result = MakeTaskLink(options.executable, task, &link);
            if (SUCCEEDED(result)) result = collection->AddObject(link.Get());
        }
        Microsoft::WRL::ComPtr<IObjectArray> array;
        if (SUCCEEDED(result)) result = collection.As(&array);
        if (SUCCEEDED(result) && !plan.tasks.empty()) result = destination->AddUserTasks(array.Get());
        if (SUCCEEDED(result)) result = destination->CommitList();
        if (FAILED(result)) return {MapError(result), result};
        active = false;
        return {ShellStatus::success, S_OK};
    } catch (...) {
        return {ShellStatus::com_failure, E_OUTOFMEMORY};
    }
}

ShellResult DeleteJumpList(std::wstring_view app_id) noexcept {
    if (!Clean(app_id)) return {ShellStatus::invalid_argument, E_INVALIDARG};
    if (!IsSta()) return {ShellStatus::wrong_apartment, CO_E_NOTINITIALIZED};
    Microsoft::WRL::ComPtr<ICustomDestinationList> destination;
    HRESULT result = ::CoCreateInstance(CLSID_DestinationList, nullptr, CLSCTX_INPROC_SERVER,
                                         IID_PPV_ARGS(&destination));
    const std::wstring id(app_id);
    if (SUCCEEDED(result)) result = destination->DeleteList(id.c_str());
    return {MapError(SUCCEEDED(result) ? S_OK : result), result};
}

ShellResult AddRecentDocument(const std::filesystem::path& path) noexcept {
    if (path.empty() || !path.is_absolute()) return {ShellStatus::invalid_argument, E_INVALIDARG};
    const std::wstring value = path.native();
    ::SHAddToRecentDocs(SHARD_PATHW, value.c_str());
    return {ShellStatus::success, S_OK};
}

ShellResult SetProcessAppUserModelId(std::wstring_view app_id) noexcept {
    if (!Clean(app_id)) return {ShellStatus::invalid_argument, E_INVALIDARG};
    const std::wstring id(app_id);
    const HRESULT result = ::SetCurrentProcessExplicitAppUserModelID(id.c_str());
    return {MapError(SUCCEEDED(result) ? S_OK : result), result};
}

ShellResult SetWindowAppUserModelId(HWND window, std::wstring_view app_id) noexcept {
    if (!window || !::IsWindow(window) || !Clean(app_id))
        return {ShellStatus::invalid_argument, E_INVALIDARG};
    Microsoft::WRL::ComPtr<IPropertyStore> properties;
    HRESULT result = ::SHGetPropertyStoreForWindow(window, IID_PPV_ARGS(&properties));
    PROPVARIANT value{};
    const std::wstring id(app_id);
    if (SUCCEEDED(result)) result = ::InitPropVariantFromString(id.c_str(), &value);
    if (SUCCEEDED(result)) result = properties->SetValue(PKEY_AppUserModel_ID, value);
    ::PropVariantClear(&value);
    if (SUCCEEDED(result)) result = properties->Commit();
    return {MapError(SUCCEEDED(result) ? S_OK : result), result};
}

}  // namespace mwtl
