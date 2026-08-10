#include <mwtl/mdi.h>

#include <stdexcept>

int main() {
    using namespace mwtl;
    MdiWorkspaceModel source;
    if (source.Add({{}, L"bad"}).status != MdiModelStatus::invalid_argument ||
        source.Add({{1}, L""}).status != MdiModelStatus::invalid_argument)
        return 1;
    if (!source.Add({{1}, L"One"}) || !source.Add({{2}, L"Two", true}) ||
        !source.Add({{3}, L"Three", false, false}) ||
        source.Add({{1}, L"duplicate"}).status != MdiModelStatus::duplicate_id)
        return 2;
    if (source.GetActiveId() != MdiChildId{3} || !source.Activate({1}) ||
        source.Activate({99}).status != MdiModelStatus::not_found)
        return 3;
    if (!source.Move({3}, 0) || source.GetChildren()[0].id != MdiChildId{3} ||
        source.Move({2}, 9).status != MdiModelStatus::invalid_argument)
        return 4;
    if (!source.Rename({1}, L"Renamed") || !source.SetDirty({1}, true) ||
        source.Find({1})->title != L"Renamed" || !source.Find({1})->dirty)
        return 5;
    if (source.Remove({3}).status != MdiModelStatus::not_closable ||
        !source.Remove({3}, true)) return 6;

    MdiWorkspaceModel destination;
    destination.Add({{8}, L"Eight"});
    if (!source.TransferTo({2}, destination, 0) ||
        source.Find({2}) || !destination.Find({2}) ||
        destination.GetChildren()[0].id != MdiChildId{2} ||
        destination.GetActiveId() != MdiChildId{2}) return 7;
    const auto source_revision = source.GetRevision();
    if (source.TransferTo({1}, source, 0).status != MdiModelStatus::invalid_argument ||
        source.GetRevision() != source_revision) return 8;

    MdiChildId routed{};
    if (!RouteMdiActiveChild(destination, [&](MdiChildId id) { routed = id; }) ||
        routed != MdiChildId{2}) return 9;
    auto reentrant = RouteMdiActiveChild(destination, [&](MdiChildId) {
        destination.Activate({8});
    });
    if (reentrant.status != MdiModelStatus::active_child_changed ||
        reentrant.child != MdiChildId{2}) return 10;
    auto failed = RouteMdiActiveChild(destination, [](MdiChildId) {
        throw std::runtime_error("route");
    });
    if (failed.status != MdiModelStatus::callback_failed || !failed.error) return 11;

    MdiWorkspaceModel bounded;
    for (std::uint64_t index = 1; index <= MdiWorkspaceModel::MaximumChildren; ++index)
        if (!bounded.Add({{index}, L"bounded"})) return 12;
    if (bounded.Add({{2000}, L"overflow"}).status != MdiModelStatus::limit_exceeded)
        return 13;
    return 0;
}
