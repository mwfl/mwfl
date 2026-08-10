#include <mwtl/docking_workspace.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace mwtl {
namespace {

template <typename Range, typename Id, typename Projection>
auto* FindById(Range& range, Id id, Projection projection) noexcept {
    const auto found = std::find_if(range.begin(), range.end(),
        [&](const auto& value) { return projection(value) == id; });
    return found == range.end() ? nullptr : &*found;
}

template <typename Range, typename Id, typename Projection>
const auto* FindById(const Range& range, Id id, Projection projection) noexcept {
    const auto found = std::find_if(range.begin(), range.end(),
        [&](const auto& value) { return projection(value) == id; });
    return found == range.end() ? nullptr : &*found;
}

DockGroupRole RoleFor(DockPanelRole role) noexcept {
    return role == DockPanelRole::document
        ? DockGroupRole::document : DockGroupRole::tool;
}

DockPanel* FindPanelIn(DockLayoutSnapshot& state, DockPanelId id) noexcept {
    return FindById(state.panels, id, [](const DockPanel& value) { return value.id; });
}

DockGroup* FindGroupIn(DockLayoutSnapshot& state, DockGroupId id) noexcept {
    return FindById(state.groups, id, [](const DockGroup& value) { return value.id; });
}

DockNode* FindNodeIn(DockLayoutSnapshot& state, DockNodeId id) noexcept {
    return FindById(state.nodes, id, [](const DockNode& value) { return value.id; });
}

bool Contains(std::span<const DockPanelId> values, DockPanelId id) noexcept {
    return std::find(values.begin(), values.end(), id) != values.end();
}

void SelectFallback(DockGroup& group) noexcept {
    if (group.active && Contains(group.panels, *group.active)) return;
    group.active = group.panels.empty()
        ? std::nullopt : std::optional{group.panels.front()};
}

void DetachPanel(DockLayoutSnapshot& state, DockPanelId id) {
    for (auto& group : state.groups) {
        const auto original_size = group.panels.size();
        std::erase(group.panels, id);
        if (group.panels.size() != original_size) SelectFallback(group);
    }
    std::erase_if(state.auto_hide,
        [id](const DockAutoHideEntry& entry) { return entry.panel == id; });
}

bool ReplaceNodeReference(DockLayoutSnapshot& state, DockNodeId old_node,
                          DockNodeId replacement) noexcept {
    if (state.main_root == old_node) {
        state.main_root = replacement;
        return true;
    }
    for (auto& host : state.floating_hosts) {
        if (host.root == old_node) {
            host.root = replacement;
            return true;
        }
    }
    for (auto& node : state.nodes) {
        if (node.kind != DockNodeKind::split) continue;
        if (node.first == old_node) {
            node.first = replacement;
            return true;
        }
        if (node.second == old_node) {
            node.second = replacement;
            return true;
        }
    }
    return false;
}

std::optional<DockPanelId> FirstPlacedPanel(const DockLayoutSnapshot& state) noexcept {
    for (const auto& group : state.groups) {
        if (group.active) return group.active;
        if (!group.panels.empty()) return group.panels.front();
    }
    if (!state.auto_hide.empty()) return state.auto_hide.front().panel;
    return std::nullopt;
}

DockLayoutStatus ApplyMutation(DockLayoutSnapshot& state,
                               const DockMutation& mutation) {
    DockPanel* panel = FindPanelIn(state, mutation.panel);
    if (!panel) return DockLayoutStatus::not_found;

    if (mutation.kind == DockMutationKind::activate) {
        state.active_panel = panel->id;
        for (auto& group : state.groups) {
            if (Contains(group.panels, panel->id)) group.active = panel->id;
        }
        return DockLayoutStatus::success;
    }

    if (mutation.kind == DockMutationKind::close_panel) {
        if (!panel->closable) return DockLayoutStatus::invalid_argument;
        const DockPanelId id = panel->id;
        DetachPanel(state, id);
        std::erase_if(state.panels,
            [id](const DockPanel& value) { return value.id == id; });
        if (state.active_panel == id) state.active_panel = FirstPlacedPanel(state);
        return DockLayoutStatus::success;
    }

    if (mutation.kind == DockMutationKind::auto_hide) {
        if (panel->role != DockPanelRole::tool)
            return DockLayoutStatus::role_mismatch;
        DetachPanel(state, panel->id);
        std::size_t order = 0;
        for (const auto& entry : state.auto_hide) {
            if (entry.edge == mutation.edge) order = std::max(order, entry.order + 1);
        }
        state.auto_hide.push_back({panel->id, mutation.edge, order});
        state.active_panel = panel->id;
        return DockLayoutStatus::success;
    }

    if (mutation.kind == DockMutationKind::move_to_group) {
        DockGroup* target = FindGroupIn(state, mutation.target_group);
        if (!target) return DockLayoutStatus::not_found;
        if (target->role != RoleFor(panel->role))
            return DockLayoutStatus::role_mismatch;
        const DockPanelId id = panel->id;
        DetachPanel(state, id);
        target = FindGroupIn(state, mutation.target_group);
        const std::size_t index = mutation.target_index
            ? std::min(*mutation.target_index, target->panels.size())
            : target->panels.size();
        target->panels.insert(target->panels.begin() +
                              static_cast<std::ptrdiff_t>(index), id);
        target->active = id;
        state.active_panel = id;
        return DockLayoutStatus::success;
    }

    if (mutation.kind != DockMutationKind::split_to_group &&
        mutation.kind != DockMutationKind::float_panel)
        return DockLayoutStatus::invalid_argument;

    if (!mutation.new_group || !mutation.new_group_node ||
        FindGroupIn(state, mutation.new_group) || FindNodeIn(state, mutation.new_group_node))
        return DockLayoutStatus::duplicate_id;

    const DockPanelId id = panel->id;
    const DockGroupRole role = RoleFor(panel->role);
    DetachPanel(state, id);
    state.groups.push_back({mutation.new_group, role, {id}, id});
    state.nodes.push_back({mutation.new_group_node, DockNodeKind::group,
                           mutation.new_group});

    if (mutation.kind == DockMutationKind::float_panel) {
        if (!mutation.new_floating_host ||
            FindById(state.floating_hosts, mutation.new_floating_host,
                [](const DockFloatingHost& value) { return value.id; }) ||
            !mutation.floating_placement.IsValid())
            return DockLayoutStatus::duplicate_id;
        state.floating_hosts.push_back({mutation.new_floating_host,
            mutation.new_group_node, mutation.floating_placement});
        state.active_panel = id;
        return DockLayoutStatus::success;
    }

    DockGroup* target_group = FindGroupIn(state, mutation.target_group);
    if (!target_group || !mutation.new_split_node ||
        FindNodeIn(state, mutation.new_split_node) ||
        !std::isfinite(mutation.split_ratio) || mutation.split_ratio < 0.1 ||
        mutation.split_ratio > 0.9)
        return target_group ? DockLayoutStatus::duplicate_id
                            : DockLayoutStatus::not_found;
    const DockNode* target_node = nullptr;
    for (const auto& node : state.nodes) {
        if (node.kind == DockNodeKind::group && node.group == target_group->id) {
            target_node = &node;
            break;
        }
    }
    if (!target_node) return DockLayoutStatus::invalid_graph;
    const DockNodeId target_node_id = target_node->id;
    const bool new_first = mutation.edge == DockEdge::left ||
                           mutation.edge == DockEdge::top;
    const DockSplitOrientation orientation =
        mutation.edge == DockEdge::left || mutation.edge == DockEdge::right
        ? DockSplitOrientation::vertical : DockSplitOrientation::horizontal;
    if (!ReplaceNodeReference(state, target_node_id, mutation.new_split_node))
        return DockLayoutStatus::invalid_graph;
    state.nodes.push_back({mutation.new_split_node, DockNodeKind::split, {},
        orientation,
        new_first ? mutation.new_group_node : target_node_id,
        new_first ? target_node_id : mutation.new_group_node,
        mutation.split_ratio});
    state.active_panel = id;
    return DockLayoutStatus::success;
}

}  // namespace

bool DockFloatingPlacement::IsValid() const noexcept {
    return std::isfinite(x) && std::isfinite(y) && std::isfinite(width) &&
           std::isfinite(height) && width >= 80.0 && height >= 60.0 &&
           width <= 100000.0 && height <= 100000.0 && monitor_key.size() <= 256;
}

bool DockLayoutTransaction::IsValid() const noexcept {
    return source_revision != 0 && mutation.panel && proposed.main_root &&
           DockLayoutModel::Validate(proposed) == DockLayoutStatus::success;
}

DockLayoutModel::DockLayoutModel(DockGroupId root_group, DockNodeId root_node,
                                 DockGroupRole root_role) {
    if (!root_group || !root_node) throw std::invalid_argument("docking root IDs must be nonzero");
    state_.main_root = root_node;
    state_.groups.push_back({root_group, root_role});
    state_.nodes.push_back({root_node, DockNodeKind::group, root_group});
    revision_ = 1;
}

DockLayoutResult DockLayoutModel::AddDockedGroup(
    DockGroupId group_id, DockNodeId group_node, DockGroupRole role,
    DockGroupId target_group_id, DockNodeId split_node, DockEdge edge,
    double ratio) {
    if (!group_id || !group_node || !split_node ||
        FindGroup(group_id) || FindNode(group_node) || FindNode(split_node))
        return {DockLayoutStatus::duplicate_id, state_.active_panel};
    if (!std::isfinite(ratio) || ratio < 0.1 || ratio > 0.9)
        return {DockLayoutStatus::invalid_argument, state_.active_panel};
    const DockGroup* target_group = FindGroup(target_group_id);
    if (!target_group) return {DockLayoutStatus::not_found, state_.active_panel};
    const DockNode* target_node = nullptr;
    for (const auto& node : state_.nodes) {
        if (node.kind == DockNodeKind::group && node.group == target_group_id) {
            target_node = &node;
            break;
        }
    }
    if (!target_node) return {DockLayoutStatus::invalid_graph, state_.active_panel};
    DockLayoutSnapshot proposed = state_;
    const DockNodeId target_node_id = target_node->id;
    proposed.groups.push_back({group_id, role});
    proposed.nodes.push_back({group_node, DockNodeKind::group, group_id});
    if (!ReplaceNodeReference(proposed, target_node_id, split_node))
        return {DockLayoutStatus::invalid_graph, state_.active_panel};
    const bool new_first = edge == DockEdge::left || edge == DockEdge::top;
    proposed.nodes.push_back({split_node, DockNodeKind::split, {},
        edge == DockEdge::left || edge == DockEdge::right
            ? DockSplitOrientation::vertical : DockSplitOrientation::horizontal,
        new_first ? group_node : target_node_id,
        new_first ? target_node_id : group_node,
        ratio});
    const DockLayoutStatus status = Validate(proposed);
    if (status != DockLayoutStatus::success) return {status, state_.active_panel};
    state_ = std::move(proposed);
    ++revision_;
    return Success();
}

DockLayoutResult DockLayoutModel::AddPanel(DockPanel panel, DockGroupId group_id,
                                           std::optional<std::size_t> index) {
    if (!panel.id || panel.title.empty()) return {DockLayoutStatus::invalid_argument, state_.active_panel};
    if (FindPanel(panel.id)) return {DockLayoutStatus::duplicate_id, state_.active_panel};
    DockGroup* group = FindGroupIn(state_, group_id);
    if (!group) return {DockLayoutStatus::not_found, state_.active_panel};
    if (group->role != RoleFor(panel.role))
        return {DockLayoutStatus::role_mismatch, state_.active_panel};
    const std::size_t target = index ? std::min(*index, group->panels.size())
                                     : group->panels.size();
    const DockPanelId id = panel.id;
    state_.panels.push_back(std::move(panel));
    group = FindGroupIn(state_, group_id);
    group->panels.insert(group->panels.begin() + static_cast<std::ptrdiff_t>(target), id);
    group->active = id;
    state_.active_panel = id;
    ++revision_;
    return Success();
}

std::optional<DockLayoutTransaction> DockLayoutModel::Propose(
    const DockMutation& mutation, DockLayoutStatus* failure) const {
    DockLayoutSnapshot proposed = state_;
    DockLayoutStatus status = ApplyMutation(proposed, mutation);
    if (status == DockLayoutStatus::success) status = Validate(proposed);
    if (failure) *failure = status;
    if (status != DockLayoutStatus::success) return std::nullopt;
    return DockLayoutTransaction{revision_, mutation, std::move(proposed)};
}

DockLayoutResult DockLayoutModel::Commit(const DockLayoutTransaction& transaction) {
    if (transaction.source_revision != revision_)
        return {DockLayoutStatus::stale_transaction, state_.active_panel};
    const DockLayoutStatus status = Validate(transaction.proposed);
    if (status != DockLayoutStatus::success) return {status, state_.active_panel};
    state_ = transaction.proposed;
    ++revision_;
    return Success();
}

DockLayoutResult DockLayoutModel::Replace(DockLayoutSnapshot snapshot) {
    const DockLayoutStatus status = Validate(snapshot);
    if (status != DockLayoutStatus::success) return {status, state_.active_panel};
    state_ = std::move(snapshot);
    ++revision_;
    return Success();
}

const DockPanel* DockLayoutModel::FindPanel(DockPanelId id) const noexcept {
    return FindById(state_.panels, id, [](const DockPanel& value) { return value.id; });
}

const DockGroup* DockLayoutModel::FindGroup(DockGroupId id) const noexcept {
    return FindById(state_.groups, id, [](const DockGroup& value) { return value.id; });
}

const DockNode* DockLayoutModel::FindNode(DockNodeId id) const noexcept {
    return FindById(state_.nodes, id, [](const DockNode& value) { return value.id; });
}

const DockFloatingHost* DockLayoutModel::FindFloatingHost(DockFloatingHostId id) const noexcept {
    return FindById(state_.floating_hosts, id,
        [](const DockFloatingHost& value) { return value.id; });
}

std::optional<DockGroupId> DockLayoutModel::FindPanelGroup(DockPanelId id) const noexcept {
    for (const auto& group : state_.groups) {
        if (Contains(group.panels, id)) return group.id;
    }
    return std::nullopt;
}

bool DockLayoutModel::IsAutoHidden(DockPanelId id) const noexcept {
    return std::any_of(state_.auto_hide.begin(), state_.auto_hide.end(),
        [id](const DockAutoHideEntry& value) { return value.panel == id; });
}

DockLayoutStatus DockLayoutModel::Validate(const DockLayoutSnapshot& state) noexcept {
    try {
        if (!state.main_root || state.panels.size() > 4096 ||
            state.groups.size() > 4096 || state.nodes.size() > 8192 ||
            state.floating_hosts.size() > 1024 || state.auto_hide.size() > 4096)
            return DockLayoutStatus::invalid_graph;

        auto duplicate = [](const auto& values, auto projection) {
            for (std::size_t i = 0; i < values.size(); ++i) {
                if (!projection(values[i])) return true;
                for (std::size_t j = i + 1; j < values.size(); ++j) {
                    if (projection(values[i]) == projection(values[j])) return true;
                }
            }
            return false;
        };
        if (duplicate(state.panels, [](const DockPanel& value) { return value.id; }) ||
            duplicate(state.groups, [](const DockGroup& value) { return value.id; }) ||
            duplicate(state.nodes, [](const DockNode& value) { return value.id; }) ||
            duplicate(state.floating_hosts, [](const DockFloatingHost& value) { return value.id; }))
            return DockLayoutStatus::duplicate_id;

        std::vector<std::size_t> panel_uses(state.panels.size());
        std::vector<std::size_t> group_uses(state.groups.size());
        auto panel_index = [&](DockPanelId id) -> std::optional<std::size_t> {
            for (std::size_t i = 0; i < state.panels.size(); ++i)
                if (state.panels[i].id == id) return i;
            return std::nullopt;
        };
        auto group_index = [&](DockGroupId id) -> std::optional<std::size_t> {
            for (std::size_t i = 0; i < state.groups.size(); ++i)
                if (state.groups[i].id == id) return i;
            return std::nullopt;
        };
        auto node_index = [&](DockNodeId id) -> std::optional<std::size_t> {
            for (std::size_t i = 0; i < state.nodes.size(); ++i)
                if (state.nodes[i].id == id) return i;
            return std::nullopt;
        };

        for (const auto& panel : state.panels) {
            if (panel.title.empty() || panel.title.size() > 4096)
                return DockLayoutStatus::invalid_argument;
        }
        for (const auto& group : state.groups) {
            for (std::size_t i = 0; i < group.panels.size(); ++i) {
                const auto index = panel_index(group.panels[i]);
                if (!index || ++panel_uses[*index] != 1 ||
                    RoleFor(state.panels[*index].role) != group.role)
                    return DockLayoutStatus::invalid_graph;
                for (std::size_t j = i + 1; j < group.panels.size(); ++j)
                    if (group.panels[i] == group.panels[j]) return DockLayoutStatus::duplicate_id;
            }
            if (group.panels.empty() != !group.active ||
                (group.active && !Contains(group.panels, *group.active)))
                return DockLayoutStatus::invalid_graph;
        }
        for (const auto& entry : state.auto_hide) {
            const auto index = panel_index(entry.panel);
            if (!index || state.panels[*index].role != DockPanelRole::tool ||
                ++panel_uses[*index] != 1) return DockLayoutStatus::invalid_graph;
        }
        if (std::any_of(panel_uses.begin(), panel_uses.end(),
                        [](std::size_t count) { return count != 1; }))
            return DockLayoutStatus::invalid_graph;
        if (state.active_panel && !panel_index(*state.active_panel))
            return DockLayoutStatus::invalid_graph;

        std::vector<std::size_t> parents(state.nodes.size());
        for (const auto& node : state.nodes) {
            if (node.kind == DockNodeKind::group) {
                const auto index = group_index(node.group);
                if (!index || ++group_uses[*index] != 1 || node.first || node.second)
                    return DockLayoutStatus::invalid_graph;
            } else {
                const auto first = node_index(node.first);
                const auto second = node_index(node.second);
                if (!first || !second || *first == *second || node.group ||
                    !std::isfinite(node.ratio) || node.ratio < 0.1 || node.ratio > 0.9)
                    return DockLayoutStatus::invalid_graph;
                ++parents[*first];
                ++parents[*second];
            }
        }
        if (std::any_of(group_uses.begin(), group_uses.end(),
                        [](std::size_t count) { return count != 1; }))
            return DockLayoutStatus::invalid_graph;

        std::vector<DockNodeId> roots{state.main_root};
        for (const auto& host : state.floating_hosts) {
            if (!host.placement.IsValid() || !node_index(host.root) ||
                std::find(roots.begin(), roots.end(), host.root) != roots.end())
                return DockLayoutStatus::invalid_graph;
            roots.push_back(host.root);
        }
        for (DockNodeId root : roots) {
            const auto index = node_index(root);
            if (!index) return DockLayoutStatus::invalid_graph;
            ++parents[*index];  // Synthetic root ownership.
        }
        if (std::any_of(parents.begin(), parents.end(),
                        [](std::size_t count) { return count != 1; }))
            return DockLayoutStatus::invalid_graph;

        std::vector<bool> visited(state.nodes.size());
        std::vector<DockNodeId> pending = roots;
        while (!pending.empty()) {
            const DockNodeId id = pending.back();
            pending.pop_back();
            const auto index = node_index(id);
            if (!index || visited[*index]) return DockLayoutStatus::invalid_graph;
            visited[*index] = true;
            const auto& node = state.nodes[*index];
            if (node.kind == DockNodeKind::split) {
                pending.push_back(node.first);
                pending.push_back(node.second);
            }
        }
        if (std::find(visited.begin(), visited.end(), false) != visited.end())
            return DockLayoutStatus::invalid_graph;
        return DockLayoutStatus::success;
    } catch (...) {
        return DockLayoutStatus::invalid_graph;
    }
}

DockLayoutResult DockLayoutModel::Success() const noexcept {
    return {DockLayoutStatus::success, state_.active_panel};
}

}  // namespace mwtl
