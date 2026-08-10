#include <mwfl/docking_session.h>
#include <mwfl/text_file.h>

#include <filesystem>

int main() {
    using namespace mwfl;
    DockLayoutModel model{{10}, {100}, DockGroupRole::document};
    if (!model.AddDockedGroup({20}, {200}, DockGroupRole::tool, {10}, {300},
                              DockEdge::left, 0.3) ||
        !model.AddPanel({{1}, L"Unicode 文档", DockPanelRole::document}, {10}) ||
        !model.AddPanel({{2}, L"Solution \"Explorer\"", DockPanelRole::tool}, {20}) ||
        !model.AddPanel({{3}, L"Output", DockPanelRole::tool}, {20})) return 1;
    DockMutation hide;
    hide.kind = DockMutationKind::auto_hide;
    hide.panel = {3};
    hide.edge = DockEdge::bottom;
    const auto hidden = model.Propose(hide);
    if (!hidden || !model.Commit(*hidden)) return 2;
    DockMutation floating;
    floating.kind = DockMutationKind::float_panel;
    floating.panel = {2};
    floating.new_group = {21};
    floating.new_group_node = {201};
    floating.new_floating_host = {400};
    floating.floating_placement = {140, 90, 600, 420, L"DISPLAY-PRIMARY"};
    const auto floated = model.Propose(floating);
    if (!floated || !model.Commit(*floated)) return 3;

    const std::wstring text = SerializeDockingSession(model.GetSnapshot());
    const auto parsed = ParseDockingSession(text);
    if (!parsed || parsed.snapshot->panels.size() != 3 ||
        parsed.snapshot->groups.size() != 3 ||
        parsed.snapshot->floating_hosts.size() != 1 ||
        parsed.snapshot->auto_hide.size() != 1 ||
        parsed.snapshot->panels[0].title != L"Unicode 文档" ||
        parsed.snapshot->floating_hosts[0].placement.monitor_key != L"DISPLAY-PRIMARY")
        return 4;
    DockLayoutModel restored{{90}, {900}};
    if (!restored.Replace(*parsed.snapshot) || !restored.IsAutoHidden({3}) ||
        !restored.FindFloatingHost({400}) || restored.FindPanelGroup({2}) != DockGroupId{21})
        return 5;

    auto future = text;
    future.replace(future.find(L" 1\n"), 3, L" 2\n");
    if (ParseDockingSession(future).status != DockingSessionStatus::unsupported_version)
        return 6;
    auto migrated = text;
    migrated.replace(migrated.find(L" 1\n"), 3, L" 0\n");
    if (!ParseDockingSession(migrated)) return 7;
    auto extended = text;
    extended.insert(extended.find(L"PC "), L"X future-root-option 1\n");
    extended += L"X future-trailer value\n";
    if (!ParseDockingSession(extended)) return 8;
    if (ParseDockingSession(text.substr(0, text.size() - 2)).status !=
        DockingSessionStatus::malformed) return 9;
    auto wrong_count_tag = text;
    wrong_count_tag.replace(wrong_count_tag.find(L"PC "), 2, L"PX");
    if (ParseDockingSession(wrong_count_tag).status != DockingSessionStatus::malformed)
        return 19;

    DockingSessionLimits tiny;
    tiny.maximum_bytes = 16;
    if (ParseDockingSession(text, tiny).status != DockingSessionStatus::oversized)
        return 10;
    DockingSessionLimits few_panels;
    few_panels.maximum_panels = 2;
    if (ParseDockingSession(text, few_panels).status != DockingSessionStatus::oversized)
        return 11;

    auto duplicate = model.GetSnapshot();
    duplicate.panels.push_back(duplicate.panels.front());
    const auto duplicate_text = SerializeDockingSession(duplicate);
    if (ParseDockingSession(duplicate_text).status !=
        DockingSessionStatus::duplicate_identity) return 12;
    auto cyclic = model.GetSnapshot();
    for (auto& node : cyclic.nodes) {
        if (node.kind == DockNodeKind::split) {
            node.first = node.id;
            break;
        }
    }
    if (ParseDockingSession(SerializeDockingSession(cyclic)).status !=
        DockingSessionStatus::invalid_graph) return 13;

    const auto root = std::filesystem::temp_directory_path() /
        (L"mwfl-docking-session-" + std::to_wstring(::GetCurrentProcessId()));
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root);
    const auto path = root / L"layout.mwfl";
    if (SaveDockingSessionAtomic(path, model.GetSnapshot()) !=
        DockingSessionStatus::success) return 14;
    const auto loaded = LoadDockingSession(path);
    if (!loaded || loaded.snapshot->main_root != model.GetSnapshot().main_root)
        return 15;
    if (!WriteTextFileAtomic(path, L"MWFL_DOCK_LAYOUT 1\nR 100").Succeeded() ||
        LoadDockingSession(path).status != DockingSessionStatus::malformed)
        return 16;
    if (SaveDockingSessionAtomic(root / L"invalid.mwfl", cyclic) !=
        DockingSessionStatus::invalid_graph) return 17;
    std::filesystem::remove_all(root, ignored);
    if (std::filesystem::exists(root)) return 18;
    return 0;
}
