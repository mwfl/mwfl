#include <mwtl/docking_session.h>

#include <mwtl/text_file.h>

#include <iomanip>
#include <limits>
#include <sstream>

namespace mwtl {
namespace {

constexpr std::wstring_view magic = L"MWTL_DOCK_LAYOUT";

std::optional<std::size_t> Utf8Size(std::wstring_view text) {
    if (text.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)()))
        return std::nullopt;
    const int size = ::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
        text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    if (size == 0 && !text.empty()) return std::nullopt;
    return static_cast<std::size_t>(size);
}

bool ReadRequiredLine(std::wistream& stream, std::wstring& line) {
    while (std::getline(stream, line)) {
        if (!line.starts_with(L"X ")) return true;
    }
    return false;
}

bool Finish(std::wistringstream& line) {
    line >> std::ws;
    return line.eof();
}

DockingSessionStatus GraphStatus(DockLayoutStatus status) noexcept {
    switch (status) {
    case DockLayoutStatus::success: return DockingSessionStatus::success;
    case DockLayoutStatus::duplicate_id: return DockingSessionStatus::duplicate_identity;
    case DockLayoutStatus::invalid_argument:
    case DockLayoutStatus::role_mismatch:
    case DockLayoutStatus::invalid_graph:
    case DockLayoutStatus::not_found:
    case DockLayoutStatus::stale_transaction:
        return DockingSessionStatus::invalid_graph;
    }
    return DockingSessionStatus::invalid_graph;
}

}  // namespace

std::wstring SerializeDockingSession(const DockLayoutSnapshot& state) {
    std::wostringstream out;
    out << magic << L' ' << docking_session_schema_version << L'\n';
    out << L"R " << state.main_root.value << L' '
        << state.active_panel.value_or(DockPanelId{}).value << L'\n';
    out << L"PC " << state.panels.size() << L'\n';
    for (const auto& panel : state.panels) {
        out << L"P " << panel.id.value << L' '
            << static_cast<unsigned>(panel.role) << L' ' << panel.closable << L' '
            << std::quoted(panel.title) << L'\n';
    }
    out << L"GC " << state.groups.size() << L'\n';
    for (const auto& group : state.groups) {
        out << L"G " << group.id.value << L' '
            << static_cast<unsigned>(group.role) << L' '
            << group.active.value_or(DockPanelId{}).value << L' '
            << group.panels.size();
        for (DockPanelId panel : group.panels) out << L' ' << panel.value;
        out << L'\n';
    }
    out << L"NC " << state.nodes.size() << L'\n';
    for (const auto& node : state.nodes) {
        out << L"N " << node.id.value << L' '
            << static_cast<unsigned>(node.kind) << L' ' << node.group.value << L' '
            << static_cast<unsigned>(node.orientation) << L' '
            << node.first.value << L' ' << node.second.value << L' '
            << std::setprecision(17) << node.ratio << L'\n';
    }
    out << L"FC " << state.floating_hosts.size() << L'\n';
    for (const auto& host : state.floating_hosts) {
        out << L"F " << host.id.value << L' ' << host.root.value << L' '
            << std::setprecision(17) << host.placement.x << L' '
            << host.placement.y << L' ' << host.placement.width << L' '
            << host.placement.height << L' '
            << std::quoted(host.placement.monitor_key) << L'\n';
    }
    out << L"AC " << state.auto_hide.size() << L'\n';
    for (const auto& entry : state.auto_hide) {
        out << L"A " << entry.panel.value << L' '
            << static_cast<unsigned>(entry.edge) << L' ' << entry.order << L'\n';
    }
    out << L"E\n";
    return std::move(out).str();
}

DockingSessionResult ParseDockingSession(std::wstring_view text,
                                         const DockingSessionLimits& limits) {
    const auto bytes = Utf8Size(text);
    if (!bytes) return {DockingSessionStatus::malformed};
    if (*bytes > limits.maximum_bytes) return {DockingSessionStatus::oversized};
    std::wistringstream stream{std::wstring{text}};
    std::wstring raw;
    if (!std::getline(stream, raw)) return {DockingSessionStatus::malformed};
    std::wistringstream header{raw};
    std::wstring parsed_magic;
    std::uint32_t version = 0;
    if (!(header >> parsed_magic >> version) || parsed_magic != magic || !Finish(header))
        return {DockingSessionStatus::malformed};
    if (version > docking_session_schema_version)
        return {DockingSessionStatus::unsupported_version};

    DockLayoutSnapshot state;
    if (!ReadRequiredLine(stream, raw)) return {DockingSessionStatus::malformed};
    std::wistringstream root_line{raw};
    wchar_t root_tag{};
    std::uint64_t active = 0;
    if (!(root_line >> root_tag >> state.main_root.value >> active) ||
        root_tag != L'R' || !Finish(root_line))
        return {DockingSessionStatus::malformed};
    if (active) state.active_panel = DockPanelId{active};

    DockingSessionStatus count_failure = DockingSessionStatus::malformed;
    auto read_count = [&](std::wstring_view expected,
                          std::size_t maximum) -> std::optional<std::size_t> {
        count_failure = DockingSessionStatus::malformed;
        if (!ReadRequiredLine(stream, raw)) return std::nullopt;
        std::wistringstream line{raw};
        std::wstring tag;
        std::size_t count = 0;
        if (!(line >> tag >> count) || tag != expected || !Finish(line))
            return std::nullopt;
        if (count > maximum) {
            count_failure = DockingSessionStatus::oversized;
            return std::nullopt;
        }
        return count;
    };

    const auto panel_count = read_count(L"PC", limits.maximum_panels);
    if (!panel_count) return {count_failure};
    for (std::size_t i = 0; i < *panel_count; ++i) {
        if (!ReadRequiredLine(stream, raw)) return {DockingSessionStatus::malformed};
        std::wistringstream line{raw};
        wchar_t tag{};
        unsigned role = 0;
        DockPanel panel;
        if (!(line >> tag >> panel.id.value >> role >> panel.closable
              >> std::quoted(panel.title)) || tag != L'P' || role > 1 ||
            panel.title.size() > limits.maximum_string_units || !Finish(line))
            return {DockingSessionStatus::malformed};
        panel.role = static_cast<DockPanelRole>(role);
        state.panels.push_back(std::move(panel));
    }

    const auto group_count = read_count(L"GC", limits.maximum_groups);
    if (!group_count) return {count_failure};
    for (std::size_t i = 0; i < *group_count; ++i) {
        if (!ReadRequiredLine(stream, raw)) return {DockingSessionStatus::malformed};
        std::wistringstream line{raw};
        wchar_t tag{};
        unsigned role = 0;
        std::uint64_t active_id = 0;
        std::size_t count = 0;
        DockGroup group;
        if (!(line >> tag >> group.id.value >> role >> active_id >> count) ||
            tag != L'G' || role > 1 || count > limits.maximum_panels)
            return {DockingSessionStatus::malformed};
        group.role = static_cast<DockGroupRole>(role);
        if (active_id) group.active = DockPanelId{active_id};
        for (std::size_t p = 0; p < count; ++p) {
            DockPanelId id;
            if (!(line >> id.value)) return {DockingSessionStatus::malformed};
            group.panels.push_back(id);
        }
        if (!Finish(line)) return {DockingSessionStatus::malformed};
        state.groups.push_back(std::move(group));
    }

    const auto node_count = read_count(L"NC", limits.maximum_nodes);
    if (!node_count) return {count_failure};
    for (std::size_t i = 0; i < *node_count; ++i) {
        if (!ReadRequiredLine(stream, raw)) return {DockingSessionStatus::malformed};
        std::wistringstream line{raw};
        wchar_t tag{};
        unsigned kind = 0, orientation = 0;
        DockNode node;
        if (!(line >> tag >> node.id.value >> kind >> node.group.value >> orientation
              >> node.first.value >> node.second.value >> node.ratio) ||
            tag != L'N' || kind > 1 || orientation > 1 || !Finish(line))
            return {DockingSessionStatus::malformed};
        node.kind = static_cast<DockNodeKind>(kind);
        node.orientation = static_cast<DockSplitOrientation>(orientation);
        state.nodes.push_back(node);
    }

    const auto floating_count = read_count(L"FC", limits.maximum_floating_hosts);
    if (!floating_count) return {count_failure};
    for (std::size_t i = 0; i < *floating_count; ++i) {
        if (!ReadRequiredLine(stream, raw)) return {DockingSessionStatus::malformed};
        std::wistringstream line{raw};
        wchar_t tag{};
        DockFloatingHost host;
        if (!(line >> tag >> host.id.value >> host.root.value >> host.placement.x
              >> host.placement.y >> host.placement.width >> host.placement.height
              >> std::quoted(host.placement.monitor_key)) || tag != L'F' ||
            host.placement.monitor_key.size() > limits.maximum_string_units || !Finish(line))
            return {DockingSessionStatus::malformed};
        state.floating_hosts.push_back(std::move(host));
    }

    const auto auto_count = read_count(L"AC", limits.maximum_auto_hide);
    if (!auto_count) return {count_failure};
    for (std::size_t i = 0; i < *auto_count; ++i) {
        if (!ReadRequiredLine(stream, raw)) return {DockingSessionStatus::malformed};
        std::wistringstream line{raw};
        wchar_t tag{};
        unsigned edge = 0;
        DockAutoHideEntry entry;
        if (!(line >> tag >> entry.panel.value >> edge >> entry.order) ||
            tag != L'A' || edge > 3 || !Finish(line))
            return {DockingSessionStatus::malformed};
        entry.edge = static_cast<DockEdge>(edge);
        state.auto_hide.push_back(entry);
    }
    if (!ReadRequiredLine(stream, raw) || raw != L"E")
        return {DockingSessionStatus::malformed};
    while (std::getline(stream, raw)) {
        if (!raw.empty() && !raw.starts_with(L"X "))
            return {DockingSessionStatus::malformed};
    }
    const DockLayoutStatus graph = DockLayoutModel::Validate(state);
    if (graph != DockLayoutStatus::success) return {GraphStatus(graph)};
    return {DockingSessionStatus::success, std::move(state)};
}

DockingSessionResult LoadDockingSession(const std::filesystem::path& path,
                                        const DockingSessionLimits& limits) {
    const auto read = ReadTextFile(path);
    if (!read.Succeeded()) {
        return {read.status == TextFileStatus::not_found
                    ? DockingSessionStatus::not_found : DockingSessionStatus::io_error,
                {}, read.native_error};
    }
    if (read.value->stamp.size > limits.maximum_bytes)
        return {DockingSessionStatus::oversized};
    return ParseDockingSession(read.value->text, limits);
}

DockingSessionStatus SaveDockingSessionAtomic(
    const std::filesystem::path& path, const DockLayoutSnapshot& snapshot,
    const DockingSessionLimits& limits) {
    const DockLayoutStatus graph = DockLayoutModel::Validate(snapshot);
    if (graph != DockLayoutStatus::success) return GraphStatus(graph);
    const std::wstring serialized = SerializeDockingSession(snapshot);
    const auto bytes = Utf8Size(serialized);
    if (!bytes) return DockingSessionStatus::malformed;
    if (*bytes > limits.maximum_bytes) return DockingSessionStatus::oversized;
    const auto checked = ParseDockingSession(serialized, limits);
    if (!checked) return checked.status;
    return WriteTextFileAtomic(path, serialized).Succeeded()
        ? DockingSessionStatus::success : DockingSessionStatus::io_error;
}

}  // namespace mwtl
