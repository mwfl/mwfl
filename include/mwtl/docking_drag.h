#pragma once

#include <exception>
#include <functional>
#include <optional>
#include <span>

#include <mwtl/docking_workspace.h>

namespace mwtl {

struct DockPointDip { double x = 0.0; double y = 0.0; };
struct DockRectDip {
    double x = 0.0;
    double y = 0.0;
    double width = 0.0;
    double height = 0.0;
    bool IsValid() const noexcept;
    bool Contains(DockPointDip point) const noexcept;
};

enum class DockTargetKind { tab_group, split, auto_hide, floating };

struct DockDropTarget {
    std::uint64_t id = 0;
    DockTargetKind kind = DockTargetKind::tab_group;
    DockRectDip bounds{};
    DockGroupId group{};
    DockEdge edge = DockEdge::left;
    int priority = 0;
    bool enabled = true;
};

std::optional<DockDropTarget> HitTestDockTargets(
    DockPointDip point, std::span<const DockDropTarget> targets) noexcept;

enum class DockDragState { idle, tracking, proposed, committed, cancelled };
enum class DockDragCancelReason {
    explicit_cancel,
    escape,
    capture_lost,
    source_destroyed,
    target_destroyed,
    invalid_proposal,
    adoption_failed,
    callback_failed,
    stale_transaction,
};

enum class DockDragCommitStatus {
    success,
    invalid_state,
    adoption_failed,
    callback_failed,
    layout_failed,
};

struct DockDragCommitResult {
    DockDragCommitStatus status = DockDragCommitStatus::invalid_state;
    DockLayoutStatus layout_status = DockLayoutStatus::invalid_argument;
    std::exception_ptr error;
    explicit operator bool() const noexcept {
        return status == DockDragCommitStatus::success;
    }
};

using DockNativeAdoptHandler =
    std::function<bool(const DockLayoutSnapshot& proposed)>;
using DockNativeRollbackHandler =
    std::function<void(const DockLayoutSnapshot& original)>;

// One UI-thread drag/keyboard-dock transaction. It owns logical snapshots only.
// Preview and native HWND adoption remain application/adapter callbacks. All
// callback exceptions are contained; a failed or stale model commit invokes
// rollback with the original snapshot.
class DockDragSession final {
public:
    bool Begin(const DockLayoutModel& model, DockPanelId panel) noexcept;
    std::optional<DockDropTarget> UpdateTarget(
        DockPointDip point, std::span<const DockDropTarget> targets) noexcept;
    bool SetProposal(const DockLayoutTransaction& transaction) noexcept;
    void Cancel(DockDragCancelReason reason) noexcept;
    void Reset() noexcept;

    DockDragCommitResult Commit(DockLayoutModel& model,
                                const DockNativeAdoptHandler& adopt,
                                const DockNativeRollbackHandler& rollback) noexcept;

    DockDragState GetState() const noexcept { return state_; }
    DockPanelId GetPanel() const noexcept { return panel_; }
    std::optional<DockDropTarget> GetTarget() const noexcept { return target_; }
    std::optional<DockDragCancelReason> GetCancelReason() const noexcept {
        return cancel_reason_;
    }
    const DockLayoutTransaction* GetProposal() const noexcept {
        return proposal_ ? &*proposal_ : nullptr;
    }

private:
    DockDragState state_ = DockDragState::idle;
    DockPanelId panel_{};
    std::uint64_t source_revision_ = 0;
    DockLayoutSnapshot original_;
    std::optional<DockDropTarget> target_;
    std::optional<DockLayoutTransaction> proposal_;
    std::optional<DockDragCancelReason> cancel_reason_;
};

}  // namespace mwtl
