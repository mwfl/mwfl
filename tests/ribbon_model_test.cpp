#include <mwtl/ribbon.h>

#include <array>
#include <stdexcept>
#include <vector>

int main() {
    using namespace mwtl;
    RibbonCommandModel model;
    if (model.Add({{}, {1}}).status != RibbonModelStatus::invalid_argument ||
        model.Add({{10}, {}, 1}).status != RibbonModelStatus::invalid_argument ||
        model.Add({{10}, {1}, 0}).status != RibbonModelStatus::invalid_argument)
        return 1;
    if (!model.Add({{10}, {1}, 1}) || !model.Add({{20}, {2}, 2, {7}}) ||
        model.Add({{10}, {3}}).status != RibbonModelStatus::duplicate_id ||
        model.Add({{30}, {1}}).status != RibbonModelStatus::duplicate_id)
        return 2;
    if (!model.Find(RibbonCommandId{10}) ||
        model.Find(RibbonCommandId{10})->command != ControlId{1} ||
        model.Find(ControlId{2}) != RibbonCommandId{20}) return 3;

    int invoked = 0;
    CommandSet commands;
    commands.Add(Command({1}, L"Open", [&] { ++invoked; }).SetChecked(true))
            .Add(Command({2}, L"Context", [] {}).SetImageIndex(4));
    auto projection = model.Project(commands);
    if (!projection || projection.commands.size() != 2 ||
        projection.commands[0].label != L"Open" ||
        !projection.commands[0].checked || !projection.commands[0].visible ||
        projection.commands[1].visible) return 4;
    if (!model.SetActiveModes(2) || !model.SetContextVisible({7}, true)) return 5;
    projection = model.Project(commands);
    if (projection.commands[0].visible || !projection.commands[1].visible ||
        projection.commands[1].image_index != 4) return 6;
    if (!model.Execute({10}, commands) || invoked != 1 ||
        model.Execute({99}, commands).status != RibbonModelStatus::not_found)
        return 7;
    commands.Find({1})->SetHandler([&] {
        ++invoked;
        model.SetActiveModes(2);
    });
    if (!model.SetActiveModes(1) || !model.Execute({10}, commands) ||
        invoked != 2 || model.GetActiveModes() != 2) return 16;
    commands.Find({1})->SetHandler([] { throw std::runtime_error("command"); });
    const auto callback = model.Execute({10}, commands);
    if (callback.status != RibbonModelStatus::callback_failed || !callback.error)
        return 8;

    const std::array recent{
        RibbonRecentItem{L"one", L"One", L"C:\\one.txt"},
        RibbonRecentItem{L"two", L"Two", L"C:\\two.txt"}};
    if (!model.SetRecentItems(recent) || model.GetRecentItems().size() != 2)
        return 9;
    auto duplicate = recent;
    duplicate[1].id = L"one";
    if (model.SetRecentItems(duplicate).status != RibbonModelStatus::invalid_argument)
        return 10;
    std::vector<RibbonRecentItem> too_many(
        RibbonCommandModel::MaximumRecentItems + 1, {L"id", L"label", L""});
    if (model.SetRecentItems(too_many).status != RibbonModelStatus::limit_exceeded ||
        model.SetActiveModes(0).status != RibbonModelStatus::invalid_argument ||
        model.SetContextVisible({}, true).status != RibbonModelStatus::invalid_argument)
        return 11;
    if (!model.Remove({20}) || model.Remove({20}).status != RibbonModelStatus::not_found)
        return 12;

    RibbonCommandModel missing_model;
    missing_model.Add({{90}, {90}});
    if (missing_model.Project(commands).status != RibbonModelStatus::command_missing ||
        missing_model.Execute({90}, commands).status != RibbonModelStatus::command_missing)
        return 13;

    RibbonCommandModel bounded;
    for (std::uint32_t index = 1;
         index <= RibbonCommandModel::MaximumBindings; ++index) {
        if (!bounded.Add({{index}, {static_cast<int>(index)}, 1})) return 14;
    }
    if (bounded.Add({{5000}, {5000}, 1}).status !=
        RibbonModelStatus::limit_exceeded) return 15;
    return 0;
}
