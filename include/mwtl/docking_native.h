#pragma once

#include <windows.h>

#include <optional>
#include <vector>

#include <mwtl/docking_workspace.h>

namespace mwtl {

enum class DockNativeStatus {
    success,
    invalid_argument,
    duplicate_binding,
    not_found,
    wrong_thread_or_process,
    stale_window,
    native_failure,
    invalid_state,
};

struct DockNativePanelMove {
    DockPanelId panel{};
    HWND window = nullptr;
    HWND original_parent = nullptr;
    HWND destination_parent = nullptr;
    LONG_PTR original_style = 0;
    LONG_PTR original_extended_style = 0;
    bool original_visible = false;
    bool destination_visible = false;
};

struct DockNativeAdoptionPlan {
    std::vector<DockNativePanelMove> moves;
    HWND original_focus = nullptr;
    std::optional<DockPanelId> destination_focus;
    bool adopted = false;
    bool committed = false;
    bool IsValid() const noexcept;
};

struct DockNativeResult {
    DockNativeStatus status = DockNativeStatus::invalid_argument;
    DWORD native_error = ERROR_SUCCESS;
    explicit operator bool() const noexcept {
        return status == DockNativeStatus::success;
    }
};

// Borrows one parking parent, group-host HWNDs, and application-owned panel
// HWNDs. Every HWND must belong to this process and the attaching UI thread.
// Prepare is non-mutating; Adopt can be rolled back until Commit. Destruction
// never destroys, reparents, or otherwise owns a bound HWND.
class DockNativeWorkspaceAdapter final {
public:
    DockNativeWorkspaceAdapter() noexcept = default;
    DockNativeWorkspaceAdapter(const DockNativeWorkspaceAdapter&) = delete;
    DockNativeWorkspaceAdapter& operator=(const DockNativeWorkspaceAdapter&) = delete;

    DockNativeResult Attach(HWND parking_parent) noexcept;
    void Detach() noexcept;
    DockNativeResult BindGroup(DockGroupId group, HWND host) noexcept;
    DockNativeResult BindPanel(DockPanelId panel, HWND window) noexcept;
    DockNativeResult UnbindGroup(DockGroupId group) noexcept;
    DockNativeResult UnbindPanel(DockPanelId panel) noexcept;

    std::optional<DockNativeAdoptionPlan> Prepare(
        const DockLayoutSnapshot& snapshot,
        DockNativeStatus* failure = nullptr) const;
    DockNativeResult Adopt(DockNativeAdoptionPlan& plan) noexcept;
    DockNativeResult Rollback(DockNativeAdoptionPlan& plan) noexcept;
    DockNativeResult Commit(DockNativeAdoptionPlan& plan) noexcept;
    DockNativeResult Synchronize(const DockLayoutSnapshot& snapshot) noexcept;

    HWND FindPanel(DockPanelId panel) const noexcept;
    HWND FindGroup(DockGroupId group) const noexcept;
    HWND GetParkingParent() const noexcept { return parking_parent_; }

private:
    struct GroupBinding { DockGroupId id{}; HWND host = nullptr; };
    struct PanelBinding { DockPanelId id{}; HWND window = nullptr; };
    bool IsCompatibleWindow(HWND window) const noexcept;

    HWND parking_parent_ = nullptr;
    DWORD thread_id_ = 0;
    DWORD process_id_ = 0;
    std::vector<GroupBinding> groups_;
    std::vector<PanelBinding> panels_;
};

}  // namespace mwtl
