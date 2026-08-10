#include <mwtl/docking_workspace.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>

int main() {
    using namespace mwtl;

    bool threw = false;
    try { DockLayoutModel invalid{{}, {100}}; }
    catch (const std::invalid_argument&) { threw = true; }
    if (!threw) return 1;

    DockLayoutModel model{{10}, {100}, DockGroupRole::document};
    if (DockLayoutModel::Validate(model.GetSnapshot()) != DockLayoutStatus::success ||
        !model.AddDockedGroup({20}, {200}, DockGroupRole::tool, {10}, {300},
                              DockEdge::left, 0.25)) return 2;
    if (model.AddDockedGroup({20}, {201}, DockGroupRole::tool, {10}, {301},
                             DockEdge::right).status != DockLayoutStatus::duplicate_id)
        return 3;

    if (!model.AddPanel({{1}, L"main.cpp", DockPanelRole::document}, {10}) ||
        !model.AddPanel({{2}, L"README.md", DockPanelRole::document, false}, {10}) ||
        !model.AddPanel({{3}, L"Solution Explorer", DockPanelRole::tool}, {20}) ||
        !model.AddPanel({{4}, L"Output", DockPanelRole::tool}, {20})) return 4;
    if (model.AddPanel({{5}, L"Wrong", DockPanelRole::tool}, {10}).status !=
        DockLayoutStatus::role_mismatch) return 5;
    if (model.AddPanel({{1}, L"Duplicate", DockPanelRole::document}, {10}).status !=
        DockLayoutStatus::duplicate_id) return 6;

    const std::uint64_t before_move = model.GetRevision();
    DockMutation move;
    move.kind = DockMutationKind::move_to_group;
    move.panel = {3};
    move.target_group = {20};
    move.target_index = 1;
    DockLayoutStatus failure = DockLayoutStatus::invalid_argument;
    auto move_plan = model.Propose(move, &failure);
    if (!move_plan || failure != DockLayoutStatus::success ||
        model.GetRevision() != before_move ||
        model.FindGroup({20})->panels.front() != DockPanelId{3}) return 7;
    // A proposal is non-destructive until native adoption succeeds.
    if (move_plan->proposed.groups[1].panels[1] != DockPanelId{3}) return 8;
    if (!model.Commit(*move_plan) || model.GetRevision() != before_move + 1 ||
        model.FindGroup({20})->panels[1] != DockPanelId{3}) return 9;

    DockMutation split;
    split.kind = DockMutationKind::split_to_group;
    split.panel = {4};
    split.target_group = {20};
    split.edge = DockEdge::bottom;
    split.new_group = {21};
    split.new_group_node = {201};
    split.new_split_node = {301};
    split.split_ratio = 0.7;
    auto split_plan = model.Propose(split, &failure);
    if (!split_plan || !split_plan->IsValid() || !model.Commit(*split_plan) ||
        model.FindPanelGroup({4}) != DockGroupId{21}) return 10;
    const DockNode* nested = model.FindNode({301});
    if (!nested || nested->kind != DockNodeKind::split ||
        nested->orientation != DockSplitOrientation::horizontal ||
        nested->second != DockNodeId{201}) return 11;

    DockMutation activate;
    activate.kind = DockMutationKind::activate;
    activate.panel = {3};
    auto stale = model.Propose(activate);
    activate.panel = {1};
    auto current = model.Propose(activate);
    if (!stale || !current || !model.Commit(*current) ||
        model.Commit(*stale).status != DockLayoutStatus::stale_transaction ||
        model.GetSnapshot().active_panel != DockPanelId{1}) return 12;

    DockMutation hide;
    hide.kind = DockMutationKind::auto_hide;
    hide.panel = {3};
    hide.edge = DockEdge::left;
    auto hidden = model.Propose(hide);
    if (!hidden || !model.Commit(*hidden) || !model.IsAutoHidden({3}) ||
        model.FindPanelGroup({3})) return 13;
    hide.panel = {1};
    if (model.Propose(hide, &failure) || failure != DockLayoutStatus::role_mismatch)
        return 14;

    DockMutation floating;
    floating.kind = DockMutationKind::float_panel;
    floating.panel = {3};
    floating.new_group = {22};
    floating.new_group_node = {202};
    floating.new_floating_host = {400};
    floating.floating_placement = {120, 90, 640, 480, L"display-primary"};
    auto float_plan = model.Propose(floating);
    if (!float_plan || !model.Commit(*float_plan) || model.IsAutoHidden({3}) ||
        model.FindPanelGroup({3}) != DockGroupId{22} ||
        !model.FindFloatingHost({400})) return 15;

    floating.panel = {4};
    floating.new_group = {23};
    floating.new_group_node = {203};
    floating.new_floating_host = {401};
    floating.floating_placement.width = std::nan("");
    if (model.Propose(floating, &failure) || failure == DockLayoutStatus::success)
        return 16;

    DockMutation close;
    close.kind = DockMutationKind::close_panel;
    close.panel = {2};
    if (model.Propose(close, &failure) || failure != DockLayoutStatus::invalid_argument)
        return 17;
    close.panel = {1};
    auto close_plan = model.Propose(close);
    if (!close_plan || !model.Commit(*close_plan) || model.FindPanel({1}) ||
        model.GetSnapshot().active_panel == DockPanelId{1}) return 18;

    DockLayoutSnapshot bad = model.GetSnapshot();
    auto split_node = std::find_if(bad.nodes.begin(), bad.nodes.end(),
        [](const DockNode& node) { return node.kind == DockNodeKind::split; });
    if (split_node == bad.nodes.end()) return 19;
    split_node->first = split_node->id;
    if (DockLayoutModel::Validate(bad) != DockLayoutStatus::invalid_graph ||
        model.Replace(std::move(bad)).status != DockLayoutStatus::invalid_graph)
        return 20;

    DockLayoutSnapshot duplicate = model.GetSnapshot();
    duplicate.panels.push_back(duplicate.panels.front());
    if (DockLayoutModel::Validate(duplicate) != DockLayoutStatus::duplicate_id)
        return 21;
    return 0;
}
