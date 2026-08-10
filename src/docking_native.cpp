#include <mwfl/docking_native.h>

#include <algorithm>

namespace mwfl {
namespace {

DockNativeResult Result(DockNativeStatus status, DWORD error = ERROR_SUCCESS) noexcept {
    return {status, error};
}

bool ContainsPanel(const DockGroup& group, DockPanelId panel) noexcept {
    return std::find(group.panels.begin(), group.panels.end(), panel) !=
           group.panels.end();
}

}  // namespace

bool DockNativeAdoptionPlan::IsValid() const noexcept {
    if (moves.empty() || committed) return false;
    for (std::size_t i = 0; i < moves.size(); ++i) {
        const auto& move = moves[i];
        if (!move.panel || !move.window || !move.original_parent ||
            !move.destination_parent) return false;
        for (std::size_t j = i + 1; j < moves.size(); ++j) {
            if (move.panel == moves[j].panel || move.window == moves[j].window)
                return false;
        }
    }
    return true;
}

DockNativeResult DockNativeWorkspaceAdapter::Attach(HWND parking_parent) noexcept {
    if (parking_parent_ || !parking_parent || !::IsWindow(parking_parent))
        return Result(DockNativeStatus::invalid_argument, ERROR_INVALID_PARAMETER);
    DWORD process = 0;
    const DWORD thread = ::GetWindowThreadProcessId(parking_parent, &process);
    if (thread != ::GetCurrentThreadId() || process != ::GetCurrentProcessId())
        return Result(DockNativeStatus::wrong_thread_or_process, ERROR_INVALID_THREAD_ID);
    parking_parent_ = parking_parent;
    thread_id_ = thread;
    process_id_ = process;
    return Result(DockNativeStatus::success);
}

void DockNativeWorkspaceAdapter::Detach() noexcept {
    groups_.clear();
    panels_.clear();
    parking_parent_ = nullptr;
    thread_id_ = 0;
    process_id_ = 0;
}

bool DockNativeWorkspaceAdapter::IsCompatibleWindow(HWND window) const noexcept {
    if (!parking_parent_ || !window || !::IsWindow(window)) return false;
    DWORD process = 0;
    return ::GetWindowThreadProcessId(window, &process) == thread_id_ &&
           process == process_id_ && thread_id_ == ::GetCurrentThreadId();
}

DockNativeResult DockNativeWorkspaceAdapter::BindGroup(DockGroupId id, HWND host) noexcept {
    if (!id || !IsCompatibleWindow(host))
        return Result(DockNativeStatus::wrong_thread_or_process, ERROR_INVALID_WINDOW_HANDLE);
    if (FindGroup(id) || std::any_of(groups_.begin(), groups_.end(),
        [host](const GroupBinding& value) { return value.host == host; }))
        return Result(DockNativeStatus::duplicate_binding, ERROR_ALREADY_EXISTS);
    try { groups_.push_back({id, host}); }
    catch (...) { return Result(DockNativeStatus::native_failure, ERROR_NOT_ENOUGH_MEMORY); }
    return Result(DockNativeStatus::success);
}

DockNativeResult DockNativeWorkspaceAdapter::BindPanel(DockPanelId id, HWND window) noexcept {
    if (!id || !IsCompatibleWindow(window))
        return Result(DockNativeStatus::wrong_thread_or_process, ERROR_INVALID_WINDOW_HANDLE);
    if (FindPanel(id) || std::any_of(panels_.begin(), panels_.end(),
        [window](const PanelBinding& value) { return value.window == window; }))
        return Result(DockNativeStatus::duplicate_binding, ERROR_ALREADY_EXISTS);
    try { panels_.push_back({id, window}); }
    catch (...) { return Result(DockNativeStatus::native_failure, ERROR_NOT_ENOUGH_MEMORY); }
    return Result(DockNativeStatus::success);
}

DockNativeResult DockNativeWorkspaceAdapter::UnbindGroup(DockGroupId id) noexcept {
    const auto found = std::find_if(groups_.begin(), groups_.end(),
        [id](const GroupBinding& value) { return value.id == id; });
    if (found == groups_.end()) return Result(DockNativeStatus::not_found, ERROR_NOT_FOUND);
    groups_.erase(found);
    return Result(DockNativeStatus::success);
}

DockNativeResult DockNativeWorkspaceAdapter::UnbindPanel(DockPanelId id) noexcept {
    const auto found = std::find_if(panels_.begin(), panels_.end(),
        [id](const PanelBinding& value) { return value.id == id; });
    if (found == panels_.end()) return Result(DockNativeStatus::not_found, ERROR_NOT_FOUND);
    panels_.erase(found);
    return Result(DockNativeStatus::success);
}

std::optional<DockNativeAdoptionPlan> DockNativeWorkspaceAdapter::Prepare(
    const DockLayoutSnapshot& snapshot, DockNativeStatus* failure) const {
    auto fail = [&](DockNativeStatus status) -> std::optional<DockNativeAdoptionPlan> {
        if (failure) *failure = status;
        return std::nullopt;
    };
    if (!IsCompatibleWindow(parking_parent_) ||
        DockLayoutModel::Validate(snapshot) != DockLayoutStatus::success)
        return fail(DockNativeStatus::invalid_argument);
    DockNativeAdoptionPlan plan;
    try { plan.moves.reserve(snapshot.panels.size() + panels_.size()); }
    catch (...) { return fail(DockNativeStatus::native_failure); }
    plan.original_focus = ::GetFocus();
    plan.destination_focus = snapshot.active_panel;
    for (const auto& panel : snapshot.panels) {
        const HWND window = FindPanel(panel.id);
        if (!window) return fail(DockNativeStatus::not_found);
        if (!IsCompatibleWindow(window)) return fail(DockNativeStatus::stale_window);
        HWND destination = parking_parent_;
        bool visible = false;
        bool placed = false;
        for (const auto& group : snapshot.groups) {
            if (!ContainsPanel(group, panel.id)) continue;
            destination = FindGroup(group.id);
            if (!destination) return fail(DockNativeStatus::not_found);
            if (!IsCompatibleWindow(destination)) return fail(DockNativeStatus::stale_window);
            visible = group.active == panel.id;
            placed = true;
            break;
        }
        if (!placed) {
            placed = std::any_of(snapshot.auto_hide.begin(), snapshot.auto_hide.end(),
                [&](const DockAutoHideEntry& entry) { return entry.panel == panel.id; });
        }
        if (!placed) return fail(DockNativeStatus::invalid_argument);
        const HWND original_parent = ::GetParent(window);
        if (!original_parent || !IsCompatibleWindow(original_parent))
            return fail(DockNativeStatus::stale_window);
        try {
            plan.moves.push_back({panel.id, window, original_parent, destination,
                ::GetWindowLongPtrW(window, GWL_STYLE),
                ::GetWindowLongPtrW(window, GWL_EXSTYLE),
                ::IsWindowVisible(window) != FALSE, visible});
        } catch (...) { return fail(DockNativeStatus::native_failure); }
    }
    // Bound application HWNDs intentionally absent from the candidate snapshot
    // are closed logically. Park and hide them as part of the same adoption so
    // rollback can restore their exact parent/style/visibility/focus state.
    for (const auto& binding : panels_) {
        const bool remains = std::any_of(snapshot.panels.begin(), snapshot.panels.end(),
            [&](const DockPanel& panel) { return panel.id == binding.id; });
        if (remains) continue;
        if (!IsCompatibleWindow(binding.window))
            return fail(DockNativeStatus::stale_window);
        const HWND original_parent = ::GetParent(binding.window);
        if (!original_parent || !IsCompatibleWindow(original_parent))
            return fail(DockNativeStatus::stale_window);
        try {
            plan.moves.push_back({binding.id, binding.window, original_parent,
                parking_parent_, ::GetWindowLongPtrW(binding.window, GWL_STYLE),
                ::GetWindowLongPtrW(binding.window, GWL_EXSTYLE),
                ::IsWindowVisible(binding.window) != FALSE, false});
        } catch (...) { return fail(DockNativeStatus::native_failure); }
    }
    if (!plan.IsValid()) return fail(DockNativeStatus::invalid_argument);
    if (failure) *failure = DockNativeStatus::success;
    return plan;
}

DockNativeResult DockNativeWorkspaceAdapter::Adopt(DockNativeAdoptionPlan& plan) noexcept {
    if (!plan.IsValid() || plan.adopted || ::GetCurrentThreadId() != thread_id_)
        return Result(DockNativeStatus::invalid_state, ERROR_INVALID_STATE);
    std::size_t adopted = 0;
    for (; adopted < plan.moves.size(); ++adopted) {
        auto& move = plan.moves[adopted];
        if (!IsCompatibleWindow(move.window) || !IsCompatibleWindow(move.destination_parent))
            break;
        ::SetLastError(ERROR_SUCCESS);
        const HWND previous = ::SetParent(move.window, move.destination_parent);
        if (!previous && ::GetLastError() != ERROR_SUCCESS) break;
        const LONG_PTR style = (move.original_style | WS_CHILD) & ~WS_POPUP;
        ::SetLastError(ERROR_SUCCESS);
        if (!::SetWindowLongPtrW(move.window, GWL_STYLE, style) &&
            ::GetLastError() != ERROR_SUCCESS) break;
        const UINT flags = SWP_NOACTIVATE | SWP_NOZORDER | SWP_FRAMECHANGED |
            (move.destination_visible ? SWP_SHOWWINDOW : SWP_HIDEWINDOW);
        if (!::SetWindowPos(move.window, nullptr, 0, 0, 0, 0,
                flags | SWP_NOMOVE | SWP_NOSIZE)) break;
    }
    if (adopted != plan.moves.size()) {
        plan.adopted = true;
        Rollback(plan);
        return Result(DockNativeStatus::native_failure,
                      ::GetLastError() == ERROR_SUCCESS ? ERROR_INVALID_WINDOW_HANDLE
                                                        : ::GetLastError());
    }
    plan.adopted = true;
    if (plan.destination_focus) {
        const HWND focus = FindPanel(*plan.destination_focus);
        if (focus && ::IsWindowVisible(focus)) ::SetFocus(focus);
    }
    return Result(DockNativeStatus::success);
}

DockNativeResult DockNativeWorkspaceAdapter::Rollback(DockNativeAdoptionPlan& plan) noexcept {
    if (!plan.IsValid() || !plan.adopted || plan.committed)
        return Result(DockNativeStatus::invalid_state, ERROR_INVALID_STATE);
    bool succeeded = true;
    for (auto item = plan.moves.rbegin(); item != plan.moves.rend(); ++item) {
        if (!IsCompatibleWindow(item->window) || !IsCompatibleWindow(item->original_parent)) {
            succeeded = false;
            continue;
        }
        ::SetLastError(ERROR_SUCCESS);
        if (!::SetParent(item->window, item->original_parent) &&
            ::GetLastError() != ERROR_SUCCESS) succeeded = false;
        ::SetWindowLongPtrW(item->window, GWL_STYLE, item->original_style);
        ::SetWindowLongPtrW(item->window, GWL_EXSTYLE, item->original_extended_style);
        const UINT flags = SWP_NOACTIVATE | SWP_NOZORDER | SWP_FRAMECHANGED |
            (item->original_visible ? SWP_SHOWWINDOW : SWP_HIDEWINDOW);
        if (!::SetWindowPos(item->window, nullptr, 0, 0, 0, 0,
                flags | SWP_NOMOVE | SWP_NOSIZE)) succeeded = false;
    }
    if (plan.original_focus && IsCompatibleWindow(plan.original_focus))
        ::SetFocus(plan.original_focus);
    plan.adopted = false;
    return Result(succeeded ? DockNativeStatus::success : DockNativeStatus::native_failure,
                  succeeded ? ERROR_SUCCESS : ERROR_INVALID_WINDOW_HANDLE);
}

DockNativeResult DockNativeWorkspaceAdapter::Commit(DockNativeAdoptionPlan& plan) noexcept {
    if (!plan.IsValid() || !plan.adopted || plan.committed)
        return Result(DockNativeStatus::invalid_state, ERROR_INVALID_STATE);
    plan.committed = true;
    return Result(DockNativeStatus::success);
}

DockNativeResult DockNativeWorkspaceAdapter::Synchronize(
    const DockLayoutSnapshot& snapshot) noexcept {
    DockNativeStatus failure{};
    auto plan = Prepare(snapshot, &failure);
    if (!plan) return Result(failure);
    auto adopted = Adopt(*plan);
    if (!adopted) return adopted;
    return Commit(*plan);
}

HWND DockNativeWorkspaceAdapter::FindPanel(DockPanelId id) const noexcept {
    const auto found = std::find_if(panels_.begin(), panels_.end(),
        [id](const PanelBinding& value) { return value.id == id; });
    return found == panels_.end() ? nullptr : found->window;
}

HWND DockNativeWorkspaceAdapter::FindGroup(DockGroupId id) const noexcept {
    const auto found = std::find_if(groups_.begin(), groups_.end(),
        [id](const GroupBinding& value) { return value.id == id; });
    return found == groups_.end() ? nullptr : found->host;
}

}  // namespace mwfl
