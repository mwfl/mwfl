#include <mwfl/ribbon.h>

#include <algorithm>
#include <propvarutil.h>
#include <unordered_set>

#include <wrl/implements.h>

namespace mwfl {
namespace {

RibbonModelResult Result(RibbonModelStatus status) noexcept { return {status}; }

bool ValidText(std::wstring_view value) noexcept {
    return !value.empty() && value.size() <= 32768;
}

}  // namespace

const RibbonCommandBinding* RibbonCommandModel::Find(
    RibbonCommandId ribbon) const noexcept {
    const auto found = std::find_if(bindings_.begin(), bindings_.end(),
        [ribbon](const RibbonCommandBinding& binding) {
            return binding.ribbon == ribbon;
        });
    return found == bindings_.end() ? nullptr : &*found;
}

std::optional<RibbonCommandId> RibbonCommandModel::Find(
    ControlId command) const noexcept {
    const auto found = std::find_if(bindings_.begin(), bindings_.end(),
        [command](const RibbonCommandBinding& binding) {
            return binding.command == command;
        });
    return found == bindings_.end() ? std::nullopt
                                    : std::optional{found->ribbon};
}

RibbonModelResult RibbonCommandModel::Add(
    RibbonCommandBinding binding) noexcept {
    if (!binding.ribbon || binding.command.value <= 0 || binding.modes == 0)
        return Result(RibbonModelStatus::invalid_argument);
    if (Find(binding.ribbon) || Find(binding.command))
        return Result(RibbonModelStatus::duplicate_id);
    if (bindings_.size() >= MaximumBindings)
        return Result(RibbonModelStatus::limit_exceeded);
    try { bindings_.push_back(binding); }
    catch (...) { return {RibbonModelStatus::limit_exceeded, std::current_exception()}; }
    return Result(RibbonModelStatus::success);
}

RibbonModelResult RibbonCommandModel::Remove(RibbonCommandId ribbon) noexcept {
    const auto found = std::find_if(bindings_.begin(), bindings_.end(),
        [ribbon](const RibbonCommandBinding& binding) {
            return binding.ribbon == ribbon;
        });
    if (found == bindings_.end()) return Result(RibbonModelStatus::not_found);
    bindings_.erase(found);
    return Result(RibbonModelStatus::success);
}

RibbonModelResult RibbonCommandModel::SetActiveModes(std::uint32_t modes) noexcept {
    if (modes == 0) return Result(RibbonModelStatus::invalid_argument);
    active_modes_ = modes;
    return Result(RibbonModelStatus::success);
}

RibbonModelResult RibbonCommandModel::SetContextVisible(
    RibbonContextId context, bool visible) noexcept {
    if (context.value == 0) return Result(RibbonModelStatus::invalid_argument);
    const auto found = std::find_if(contexts_.begin(), contexts_.end(),
        [context](const ContextState& value) { return value.id == context; });
    if (found != contexts_.end()) {
        found->visible = visible;
        return Result(RibbonModelStatus::success);
    }
    if (contexts_.size() >= MaximumBindings)
        return Result(RibbonModelStatus::limit_exceeded);
    try { contexts_.push_back({context, visible}); }
    catch (...) { return {RibbonModelStatus::limit_exceeded, std::current_exception()}; }
    return Result(RibbonModelStatus::success);
}

bool RibbonCommandModel::IsContextVisible(RibbonContextId context) const noexcept {
    if (context.value == 0) return true;
    const auto found = std::find_if(contexts_.begin(), contexts_.end(),
        [context](const ContextState& value) { return value.id == context; });
    return found != contexts_.end() && found->visible;
}

RibbonModelResult RibbonCommandModel::SetRecentItems(
    std::span<const RibbonRecentItem> items) noexcept {
    if (items.size() > MaximumRecentItems)
        return Result(RibbonModelStatus::limit_exceeded);
    try {
        std::unordered_set<std::wstring> ids;
        ids.reserve(items.size());
        for (const auto& item : items) {
            if (!ValidText(item.id) || !ValidText(item.label) ||
                item.path.size() > 32768 || !ids.insert(item.id).second)
                return Result(RibbonModelStatus::invalid_argument);
        }
        recent_items_.assign(items.begin(), items.end());
    } catch (...) {
        return {RibbonModelStatus::limit_exceeded, std::current_exception()};
    }
    return Result(RibbonModelStatus::success);
}

RibbonProjectionResult RibbonCommandModel::Project(
    const CommandSet& commands) const noexcept {
    RibbonProjectionResult result{RibbonModelStatus::success};
    try {
        result.commands.reserve(bindings_.size());
        for (const auto& binding : bindings_) {
            const Command* command = commands.Find(binding.command);
            if (!command) return {RibbonModelStatus::command_missing};
            result.commands.push_back({binding.ribbon, binding.command,
                std::wstring(command->GetText()), command->IsEnabled(),
                command->IsChecked(),
                command->IsVisible() && (binding.modes & active_modes_) != 0 &&
                    IsContextVisible(binding.context),
                command->GetImageIndex()});
        }
    } catch (...) {
        return {RibbonModelStatus::limit_exceeded};
    }
    return result;
}

RibbonModelResult RibbonCommandModel::Execute(
    RibbonCommandId ribbon, const CommandSet& commands) const noexcept {
    const RibbonCommandBinding* binding = Find(ribbon);
    if (!binding) return Result(RibbonModelStatus::not_found);
    const Command* command = commands.Find(binding->command);
    if (!command) return Result(RibbonModelStatus::command_missing);
    if (!command->IsEnabled() || !command->IsVisible())
        return Result(RibbonModelStatus::invalid_argument);
    try { command->Invoke(); }
    catch (...) { return {RibbonModelStatus::callback_failed, std::current_exception()}; }
    return Result(RibbonModelStatus::success);
}

class RibbonFrameworkHost::Callback final
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IUIApplication, IUICommandHandler> {
public:
    explicit Callback(RibbonFrameworkHost* owner) noexcept : owner_(owner) {}

    void Detach() noexcept { owner_ = nullptr; }

    std::exception_ptr TakeException() noexcept {
        auto result = error_;
        error_ = nullptr;
        return result;
    }

    IFACEMETHODIMP OnViewChanged(UINT32, UI_VIEWTYPE type, IUnknown* view,
        UI_VIEWVERB verb, INT32) noexcept override {
        if (!owner_ || type != UI_VIEWTYPE_RIBBON || !view) return S_OK;
        if (verb == UI_VIEWVERB_DESTROY) {
            owner_->SetHeight(0);
            return S_OK;
        }
        Microsoft::WRL::ComPtr<IUIRibbon> ribbon;
        const HRESULT queried = view->QueryInterface(IID_PPV_ARGS(&ribbon));
        if (FAILED(queried)) return queried;
        UINT32 height = 0;
        const HRESULT result = ribbon->GetHeight(&height);
        if (SUCCEEDED(result)) owner_->SetHeight(height);
        return result;
    }

    IFACEMETHODIMP OnCreateUICommand(UINT32, UI_COMMANDTYPE,
        IUICommandHandler** handler) noexcept override {
        if (!handler) return E_POINTER;
        *handler = static_cast<IUICommandHandler*>(this);
        AddRef();
        return S_OK;
    }

    IFACEMETHODIMP OnDestroyUICommand(UINT32, UI_COMMANDTYPE,
        IUICommandHandler*) noexcept override { return S_OK; }

    IFACEMETHODIMP Execute(UINT32 command_id, UI_EXECUTIONVERB verb,
        const PROPERTYKEY*, const PROPVARIANT*, IUISimplePropertySet*) noexcept override {
        if (!owner_ || !owner_->model_ || !owner_->commands_) return E_UNEXPECTED;
        if (verb != UI_EXECUTIONVERB_EXECUTE) return S_OK;
        const auto result = owner_->model_->Execute(
            RibbonCommandId{command_id}, *owner_->commands_);
        if (result) return S_OK;
        error_ = result.error;
        return result.status == RibbonModelStatus::not_found ||
               result.status == RibbonModelStatus::command_missing
            ? HRESULT_FROM_WIN32(ERROR_NOT_FOUND) : E_FAIL;
    }

    IFACEMETHODIMP UpdateProperty(UINT32 command_id, REFPROPERTYKEY key,
        const PROPVARIANT*, PROPVARIANT* output) noexcept override {
        if (!output) return E_POINTER;
        ::PropVariantInit(output);
        if (!owner_ || !owner_->model_ || !owner_->commands_) return E_UNEXPECTED;
        const RibbonCommandBinding* binding = owner_->model_->Find(
            RibbonCommandId{command_id});
        if (!binding) return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
        const Command* command = owner_->commands_->Find(binding->command);
        if (!command) return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
        const bool mode_visible = (binding->modes & owner_->model_->GetActiveModes()) != 0;
        const bool visible = command->IsVisible() && mode_visible &&
                             owner_->model_->IsContextVisible(binding->context);
        if (IsEqualPropertyKey(key, UI_PKEY_Enabled))
            return ::InitPropVariantFromBoolean(command->IsEnabled(), output);
        if (IsEqualPropertyKey(key, UI_PKEY_BooleanValue))
            return ::InitPropVariantFromBoolean(command->IsChecked(), output);
        if (IsEqualPropertyKey(key, UI_PKEY_Label))
            return ::InitPropVariantFromString(command->GetText().data(), output);
        if (IsEqualPropertyKey(key, UI_PKEY_ContextAvailable)) {
            const UINT32 state = visible ? UI_CONTEXTAVAILABILITY_ACTIVE
                                         : UI_CONTEXTAVAILABILITY_NOTAVAILABLE;
            return ::InitPropVariantFromUInt32(state, output);
        }
        return E_NOTIMPL;
    }

private:
    RibbonFrameworkHost* owner_ = nullptr;
    std::exception_ptr error_;
};

RibbonFrameworkHost::~RibbonFrameworkHost() noexcept { Destroy(); }

RibbonHostResult RibbonFrameworkHost::CheckThread() const noexcept {
    if (!framework_) return {RibbonHostStatus::invalid_state, E_UNEXPECTED};
    if (::GetCurrentThreadId() != thread_id_)
        return {RibbonHostStatus::wrong_thread, RPC_E_WRONG_THREAD};
    return {RibbonHostStatus::success, S_OK};
}

RibbonHostResult RibbonFrameworkHost::Create(
    HWND owner, RibbonCommandModel& model, CommandSet& commands) noexcept {
    if (framework_ || !owner || !::IsWindow(owner))
        return {RibbonHostStatus::invalid_argument, E_INVALIDARG};
    DWORD process = 0;
    const DWORD thread = ::GetWindowThreadProcessId(owner, &process);
    if (thread != ::GetCurrentThreadId() || process != ::GetCurrentProcessId())
        return {RibbonHostStatus::wrong_thread, RPC_E_WRONG_THREAD};
    APTTYPE apartment{};
    APTTYPEQUALIFIER qualifier{};
    const HRESULT apartment_result = ::CoGetApartmentType(&apartment, &qualifier);
    if (FAILED(apartment_result))
        return {RibbonHostStatus::wrong_apartment, apartment_result};
    if (apartment != APTTYPE_STA && apartment != APTTYPE_MAINSTA)
        return {RibbonHostStatus::wrong_apartment, RPC_E_CHANGED_MODE};
    Microsoft::WRL::ComPtr<IUIFramework> framework;
    HRESULT result = ::CoCreateInstance(CLSID_UIRibbonFramework, nullptr,
        CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&framework));
    if (result == REGDB_E_CLASSNOTREG || result == CLASS_E_CLASSNOTAVAILABLE)
        return {RibbonHostStatus::unavailable, result};
    if (result == RPC_E_CHANGED_MODE)
        return {RibbonHostStatus::wrong_apartment, result};
    if (FAILED(result)) return {RibbonHostStatus::com_failure, result};
    auto callback = Microsoft::WRL::Make<Callback>(this);
    if (!callback) return {RibbonHostStatus::com_failure, E_OUTOFMEMORY};
    owner_ = owner;
    thread_id_ = thread;
    model_ = &model;
    commands_ = &commands;
    callback_raw_ = callback.Get();
    application_ = callback;
    result = framework->Initialize(owner, callback.Get());
    if (FAILED(result)) {
        framework->Destroy();
        callback->Detach();
        application_.Reset();
        callback_raw_ = nullptr;
        owner_ = nullptr;
        thread_id_ = 0;
        model_ = nullptr;
        commands_ = nullptr;
        return {RibbonHostStatus::com_failure, result};
    }
    framework_ = std::move(framework);
    return {RibbonHostStatus::success, S_OK};
}

RibbonHostResult RibbonFrameworkHost::Load(
    HINSTANCE module, const wchar_t* resource) noexcept {
    const auto checked = CheckThread();
    if (!checked) return checked;
    if (!module || !resource || !*resource)
        return {RibbonHostStatus::invalid_argument, E_INVALIDARG};
    const HRESULT result = framework_->LoadUI(module, resource);
    return SUCCEEDED(result) ? RibbonHostResult{RibbonHostStatus::success, result}
                             : RibbonHostResult{RibbonHostStatus::com_failure, result};
}

RibbonHostResult RibbonFrameworkHost::SetModes(std::uint32_t modes) noexcept {
    const auto checked = CheckThread();
    if (!checked) return checked;
    if (modes == 0) return {RibbonHostStatus::invalid_argument, E_INVALIDARG};
    const auto logical = model_->SetActiveModes(modes);
    if (!logical) return {RibbonHostStatus::invalid_argument, E_INVALIDARG};
    const HRESULT result = framework_->SetModes(modes);
    return SUCCEEDED(result) ? RibbonHostResult{RibbonHostStatus::success, result}
                             : RibbonHostResult{RibbonHostStatus::com_failure, result};
}

RibbonHostResult RibbonFrameworkHost::Invalidate(
    RibbonCommandId command, UI_INVALIDATIONS flags,
    const PROPERTYKEY* key) noexcept {
    const auto checked = CheckThread();
    if (!checked) return checked;
    if (!command || !model_->Find(command))
        return {RibbonHostStatus::invalid_argument, E_INVALIDARG};
    const HRESULT result = framework_->InvalidateUICommand(command.value, flags, key);
    return SUCCEEDED(result) ? RibbonHostResult{RibbonHostStatus::success, result}
                             : RibbonHostResult{RibbonHostStatus::com_failure, result};
}

RibbonHostResult RibbonFrameworkHost::InvalidateAll() noexcept {
    const auto checked = CheckThread();
    if (!checked) return checked;
    for (const auto& binding : model_->GetBindings()) {
        const HRESULT result = framework_->InvalidateUICommand(
            binding.ribbon.value, UI_INVALIDATIONS_ALLPROPERTIES, nullptr);
        if (FAILED(result)) return {RibbonHostStatus::com_failure, result};
    }
    return {RibbonHostStatus::success, S_OK};
}

std::exception_ptr RibbonFrameworkHost::TakeCallbackException() noexcept {
    return callback_raw_ ? callback_raw_->TakeException() : nullptr;
}

void RibbonFrameworkHost::Destroy() noexcept {
    if (framework_ && ::GetCurrentThreadId() != thread_id_) return;
    if (framework_) framework_->Destroy();
    if (callback_raw_) callback_raw_->Detach();
    framework_.Reset();
    application_.Reset();
    callback_raw_ = nullptr;
    owner_ = nullptr;
    thread_id_ = 0;
    model_ = nullptr;
    commands_ = nullptr;
    height_ = 0;
}

}  // namespace mwfl
