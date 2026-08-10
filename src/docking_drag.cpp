#include <mwfl/docking_drag.h>

#include <algorithm>
#include <cmath>

namespace mwfl {

bool DockRectDip::IsValid() const noexcept {
    return std::isfinite(x) && std::isfinite(y) && std::isfinite(width) &&
           std::isfinite(height) && width > 0.0 && height > 0.0 &&
           width <= 100000.0 && height <= 100000.0;
}

bool DockRectDip::Contains(DockPointDip point) const noexcept {
    return IsValid() && std::isfinite(point.x) && std::isfinite(point.y) &&
           point.x >= x && point.y >= y && point.x < x + width &&
           point.y < y + height;
}

std::optional<DockDropTarget> HitTestDockTargets(
    DockPointDip point, std::span<const DockDropTarget> targets) noexcept {
    const DockDropTarget* best = nullptr;
    for (const auto& target : targets) {
        if (!target.enabled || target.id == 0 || !target.bounds.Contains(point))
            continue;
        if ((target.kind == DockTargetKind::tab_group ||
             target.kind == DockTargetKind::split) && !target.group)
            continue;
        const double area = target.bounds.width * target.bounds.height;
        const double best_area = best ? best->bounds.width * best->bounds.height : 0.0;
        if (!best || target.priority > best->priority ||
            (target.priority == best->priority && area < best_area) ||
            (target.priority == best->priority && area == best_area &&
             target.id < best->id)) best = &target;
    }
    return best ? std::optional{*best} : std::nullopt;
}

bool DockDragSession::Begin(const DockLayoutModel& model, DockPanelId panel) noexcept {
    if (state_ != DockDragState::idle || !panel || !model.FindPanel(panel)) return false;
    try {
        original_ = model.GetSnapshot();
    } catch (...) {
        return false;
    }
    panel_ = panel;
    source_revision_ = model.GetRevision();
    state_ = DockDragState::tracking;
    return true;
}

std::optional<DockDropTarget> DockDragSession::UpdateTarget(
    DockPointDip point, std::span<const DockDropTarget> targets) noexcept {
    if (state_ != DockDragState::tracking && state_ != DockDragState::proposed)
        return std::nullopt;
    target_ = HitTestDockTargets(point, targets);
    proposal_.reset();
    state_ = DockDragState::tracking;
    return target_;
}

bool DockDragSession::SetProposal(const DockLayoutTransaction& transaction) noexcept {
    if (state_ != DockDragState::tracking || transaction.source_revision != source_revision_ ||
        transaction.mutation.panel != panel_ || !transaction.IsValid()) {
        Cancel(DockDragCancelReason::invalid_proposal);
        return false;
    }
    try {
        proposal_ = transaction;
    } catch (...) {
        Cancel(DockDragCancelReason::invalid_proposal);
        return false;
    }
    state_ = DockDragState::proposed;
    return true;
}

void DockDragSession::Cancel(DockDragCancelReason reason) noexcept {
    if (state_ == DockDragState::committed || state_ == DockDragState::idle) return;
    proposal_.reset();
    target_.reset();
    cancel_reason_ = reason;
    state_ = DockDragState::cancelled;
}

void DockDragSession::Reset() noexcept {
    state_ = DockDragState::idle;
    panel_ = {};
    source_revision_ = 0;
    original_ = {};
    target_.reset();
    proposal_.reset();
    cancel_reason_.reset();
}

DockDragCommitResult DockDragSession::Commit(
    DockLayoutModel& model, const DockNativeAdoptHandler& adopt,
    const DockNativeRollbackHandler& rollback) noexcept {
    if (state_ != DockDragState::proposed || !proposal_ || !adopt || !rollback)
        return {};
    bool adopted = false;
    try {
        adopted = adopt(proposal_->proposed);
    } catch (...) {
        const auto error = std::current_exception();
        try { rollback(original_); } catch (...) {}
        Cancel(DockDragCancelReason::callback_failed);
        return {DockDragCommitStatus::callback_failed,
                DockLayoutStatus::invalid_argument, error};
    }
    if (!adopted) {
        try { rollback(original_); } catch (...) {
            const auto error = std::current_exception();
            Cancel(DockDragCancelReason::callback_failed);
            return {DockDragCommitStatus::callback_failed,
                    DockLayoutStatus::invalid_argument, error};
        }
        Cancel(DockDragCancelReason::adoption_failed);
        return {DockDragCommitStatus::adoption_failed,
                DockLayoutStatus::invalid_argument, nullptr};
    }
    const DockLayoutResult committed = model.Commit(*proposal_);
    if (!committed) {
        std::exception_ptr rollback_error;
        try { rollback(original_); } catch (...) { rollback_error = std::current_exception(); }
        Cancel(committed.status == DockLayoutStatus::stale_transaction
                   ? DockDragCancelReason::stale_transaction
                   : DockDragCancelReason::invalid_proposal);
        return {rollback_error ? DockDragCommitStatus::callback_failed
                               : DockDragCommitStatus::layout_failed,
                committed.status, rollback_error};
    }
    proposal_.reset();
    target_.reset();
    state_ = DockDragState::committed;
    return {DockDragCommitStatus::success, DockLayoutStatus::success, nullptr};
}

}  // namespace mwfl
