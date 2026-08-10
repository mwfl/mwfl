#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace mwtl {

struct DockPanelId {
    std::uint64_t value = 0;
    constexpr auto operator<=>(const DockPanelId&) const noexcept = default;
    constexpr explicit operator bool() const noexcept { return value != 0; }
};

struct DockGroupId {
    std::uint64_t value = 0;
    constexpr auto operator<=>(const DockGroupId&) const noexcept = default;
    constexpr explicit operator bool() const noexcept { return value != 0; }
};

struct DockNodeId {
    std::uint64_t value = 0;
    constexpr auto operator<=>(const DockNodeId&) const noexcept = default;
    constexpr explicit operator bool() const noexcept { return value != 0; }
};

struct DockFloatingHostId {
    std::uint64_t value = 0;
    constexpr auto operator<=>(const DockFloatingHostId&) const noexcept = default;
    constexpr explicit operator bool() const noexcept { return value != 0; }
};

enum class DockPanelRole { document, tool };
enum class DockGroupRole { document, tool };
enum class DockNodeKind { group, split };
enum class DockSplitOrientation { vertical, horizontal };
enum class DockEdge { left, top, right, bottom };

struct DockPanel {
    DockPanelId id{};
    std::wstring title;
    DockPanelRole role = DockPanelRole::tool;
    bool closable = true;
};

struct DockGroup {
    DockGroupId id{};
    DockGroupRole role = DockGroupRole::tool;
    std::vector<DockPanelId> panels;
    std::optional<DockPanelId> active;
};

struct DockNode {
    DockNodeId id{};
    DockNodeKind kind = DockNodeKind::group;
    DockGroupId group{};
    DockSplitOrientation orientation = DockSplitOrientation::vertical;
    DockNodeId first{};
    DockNodeId second{};
    double ratio = 0.5;
};

// Logical DIPs and a stable application-defined monitor key. No HWND, HMONITOR,
// pointer, or process-local identity belongs in persisted docking state.
struct DockFloatingPlacement {
    double x = 80.0;
    double y = 80.0;
    double width = 480.0;
    double height = 360.0;
    std::wstring monitor_key;
    bool IsValid() const noexcept;
};

struct DockFloatingHost {
    DockFloatingHostId id{};
    DockNodeId root{};
    DockFloatingPlacement placement{};
};

struct DockAutoHideEntry {
    DockPanelId panel{};
    DockEdge edge = DockEdge::left;
    std::size_t order = 0;
};

struct DockLayoutSnapshot {
    DockNodeId main_root{};
    std::vector<DockPanel> panels;
    std::vector<DockGroup> groups;
    std::vector<DockNode> nodes;
    std::vector<DockFloatingHost> floating_hosts;
    std::vector<DockAutoHideEntry> auto_hide;
    std::optional<DockPanelId> active_panel;
};

enum class DockMutationKind {
    activate,
    move_to_group,
    split_to_group,
    float_panel,
    auto_hide,
    close_panel,
};

struct DockMutation {
    DockMutationKind kind = DockMutationKind::activate;
    DockPanelId panel{};
    DockGroupId target_group{};
    std::optional<std::size_t> target_index;
    DockEdge edge = DockEdge::right;
    DockGroupId new_group{};
    DockNodeId new_group_node{};
    DockNodeId new_split_node{};
    DockFloatingHostId new_floating_host{};
    DockFloatingPlacement floating_placement{};
    double split_ratio = 0.5;
};

enum class DockLayoutStatus {
    success,
    invalid_argument,
    duplicate_id,
    not_found,
    role_mismatch,
    invalid_graph,
    stale_transaction,
};

struct DockLayoutResult {
    DockLayoutStatus status = DockLayoutStatus::invalid_argument;
    std::optional<DockPanelId> active;
    explicit operator bool() const noexcept {
        return status == DockLayoutStatus::success;
    }
};

struct DockLayoutTransaction {
    std::uint64_t source_revision = 0;
    DockMutation mutation{};
    DockLayoutSnapshot proposed;
    bool IsValid() const noexcept;
};

// UI-thread-coordinated logical docking state. The model owns metadata copies
// only; applications own panel contents, commands, and HWNDs by DockPanelId.
// Propose never mutates the model. Native adoption can occur against the
// proposed snapshot, followed by Commit; abandoning the proposal is rollback.
class DockLayoutModel final {
public:
    DockLayoutModel(DockGroupId root_group, DockNodeId root_node,
                    DockGroupRole root_role = DockGroupRole::document);

    DockLayoutResult AddDockedGroup(DockGroupId group, DockNodeId group_node,
                                    DockGroupRole role, DockGroupId target_group,
                                    DockNodeId split_node, DockEdge edge,
                                    double ratio = 0.5);
    DockLayoutResult AddPanel(DockPanel panel, DockGroupId group,
                              std::optional<std::size_t> index = std::nullopt);
    std::optional<DockLayoutTransaction> Propose(const DockMutation& mutation,
                                                  DockLayoutStatus* failure = nullptr) const;
    DockLayoutResult Commit(const DockLayoutTransaction& transaction);
    DockLayoutResult Replace(DockLayoutSnapshot snapshot);

    const DockLayoutSnapshot& GetSnapshot() const noexcept { return state_; }
    std::uint64_t GetRevision() const noexcept { return revision_; }
    const DockPanel* FindPanel(DockPanelId id) const noexcept;
    const DockGroup* FindGroup(DockGroupId id) const noexcept;
    const DockNode* FindNode(DockNodeId id) const noexcept;
    const DockFloatingHost* FindFloatingHost(DockFloatingHostId id) const noexcept;
    std::optional<DockGroupId> FindPanelGroup(DockPanelId id) const noexcept;
    bool IsAutoHidden(DockPanelId id) const noexcept;

    static DockLayoutStatus Validate(const DockLayoutSnapshot& snapshot) noexcept;

private:
    DockLayoutResult Success() const noexcept;

    DockLayoutSnapshot state_;
    std::uint64_t revision_ = 0;
};

}  // namespace mwtl
