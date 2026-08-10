#include <mwtl/ribbon.h>

#include <algorithm>
#include <unordered_set>

namespace mwtl {
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

}  // namespace mwtl
