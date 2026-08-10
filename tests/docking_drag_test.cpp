#include <mwfl/docking_drag.h>

#include <array>
#include <stdexcept>

namespace {

mwfl::DockLayoutModel MakeModel() {
    mwfl::DockLayoutModel model{{10}, {100}, mwfl::DockGroupRole::tool};
    model.AddPanel({{1}, L"Explorer", mwfl::DockPanelRole::tool}, {10});
    model.AddPanel({{2}, L"Output", mwfl::DockPanelRole::tool}, {10});
    return model;
}

mwfl::DockMutation HideMutation(mwfl::DockPanelId panel) {
    mwfl::DockMutation mutation;
    mutation.kind = mwfl::DockMutationKind::auto_hide;
    mutation.panel = panel;
    mutation.edge = mwfl::DockEdge::left;
    return mutation;
}

}  // namespace

int main() {
    using namespace mwfl;
    const std::array targets{
        DockDropTarget{20, DockTargetKind::tab_group, {0, 0, 200, 200}, {10},
                       DockEdge::left, 1, true},
        DockDropTarget{10, DockTargetKind::split, {40, 40, 80, 80}, {10},
                       DockEdge::right, 1, true},
        DockDropTarget{30, DockTargetKind::auto_hide, {40, 40, 80, 80}, {},
                       DockEdge::left, 2, true},
        DockDropTarget{5, DockTargetKind::floating, {40, 40, 80, 80}, {},
                       DockEdge::left, 2, false},
    };
    const auto hit = HitTestDockTargets({60, 60}, targets);
    if (!hit || hit->id != 30 || HitTestDockTargets({250, 250}, targets)) return 1;
    auto invalid_targets = targets;
    invalid_targets[0].group = {};
    if (HitTestDockTargets({10, 10}, invalid_targets)) return 2;

    auto model = MakeModel();
    DockDragSession session;
    if (!session.Begin(model, {1}) || session.Begin(model, {2}) ||
        session.GetState() != DockDragState::tracking ||
        !session.UpdateTarget({60, 60}, targets) ||
        session.GetTarget()->id != 30) return 3;
    const auto proposal = model.Propose(HideMutation({1}));
    if (!proposal || !session.SetProposal(*proposal) ||
        session.GetState() != DockDragState::proposed) return 4;
    bool adopted = false;
    bool rolled_back = false;
    const auto committed = session.Commit(model,
        [&](const DockLayoutSnapshot& proposed) {
            adopted = proposed.auto_hide.size() == 1;
            return true;
        },
        [&](const DockLayoutSnapshot&) { rolled_back = true; });
    if (!committed || !adopted || rolled_back || !model.IsAutoHidden({1}) ||
        session.GetState() != DockDragState::committed) return 5;

    session.Reset();
    if (session.GetState() != DockDragState::idle || !session.Begin(model, {2})) return 6;
    auto rejected_plan = model.Propose(HideMutation({2}));
    if (!rejected_plan || !session.SetProposal(*rejected_plan)) return 7;
    const auto rejected = session.Commit(model,
        [](const DockLayoutSnapshot&) { return false; },
        [&](const DockLayoutSnapshot& original) {
            rolled_back = original.panels.size() == 2;
        });
    if (rejected.status != DockDragCommitStatus::adoption_failed || !rolled_back ||
        model.IsAutoHidden({2}) || session.GetCancelReason() !=
            DockDragCancelReason::adoption_failed) return 8;

    auto throwing_model = MakeModel();
    DockDragSession throwing;
    if (!throwing.Begin(throwing_model, {1})) return 9;
    auto throwing_plan = throwing_model.Propose(HideMutation({1}));
    if (!throwing_plan || !throwing.SetProposal(*throwing_plan)) return 10;
    bool throw_rollback = false;
    const auto callback_failure = throwing.Commit(throwing_model,
        [](const DockLayoutSnapshot&) -> bool { throw std::runtime_error("adopt"); },
        [&](const DockLayoutSnapshot&) { throw_rollback = true; });
    if (callback_failure.status != DockDragCommitStatus::callback_failed ||
        !callback_failure.error || !throw_rollback || throwing_model.IsAutoHidden({1}) ||
        throwing.GetCancelReason() != DockDragCancelReason::callback_failed) return 11;

    auto reentrant_model = MakeModel();
    DockDragSession reentrant;
    if (!reentrant.Begin(reentrant_model, {1})) return 12;
    auto stale_plan = reentrant_model.Propose(HideMutation({1}));
    if (!stale_plan || !reentrant.SetProposal(*stale_plan)) return 13;
    bool stale_rollback = false;
    const auto stale = reentrant.Commit(reentrant_model,
        [&](const DockLayoutSnapshot&) {
            const auto concurrent = reentrant_model.Propose(HideMutation({2}));
            return concurrent && reentrant_model.Commit(*concurrent);
        }, [&](const DockLayoutSnapshot&) { stale_rollback = true; });
    if (stale.status != DockDragCommitStatus::layout_failed ||
        stale.layout_status != DockLayoutStatus::stale_transaction || !stale_rollback ||
        !reentrant_model.IsAutoHidden({2}) || reentrant_model.IsAutoHidden({1}) ||
        reentrant.GetCancelReason() != DockDragCancelReason::stale_transaction) return 14;

    auto cancel_model = MakeModel();
    DockDragSession cancelled;
    if (!cancelled.Begin(cancel_model, {1})) return 15;
    cancelled.Cancel(DockDragCancelReason::escape);
    if (cancelled.GetState() != DockDragState::cancelled ||
        cancelled.GetCancelReason() != DockDragCancelReason::escape ||
        cancelled.Commit(cancel_model, [](const auto&) { return true; },
                           [](const auto&) {}).status !=
            DockDragCommitStatus::invalid_state) return 16;
    cancelled.Reset();
    if (cancelled.Begin(cancel_model, {99})) return 17;

    DockDragSession bad_proposal;
    if (!bad_proposal.Begin(cancel_model, {1})) return 18;
    auto other_panel = cancel_model.Propose(HideMutation({2}));
    if (!other_panel || bad_proposal.SetProposal(*other_panel) ||
        bad_proposal.GetCancelReason() != DockDragCancelReason::invalid_proposal)
        return 19;
    return 0;
}
