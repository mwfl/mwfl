#include <mwfl/mwfl.h>

#include <array>

void BuildDocumentWorkspaceFixture() {
    mwfl::DocumentWorkspaceModel left{{1}, 8};
    mwfl::DocumentWorkspaceModel right{{2}, 8};
    left.Add({{11}, L"One", L"C:\\Docs\\one.txt", true, true, false});
    left.Add({{22}, L"Two"});
    auto projection = mwfl::BuildActiveDocumentCommandProjection(left);
    mwfl::CommandSet commands;
    commands.Add(mwfl::Command({101}, L"Save"))
            .Add(mwfl::Command({102}, L"Close"))
            .Add(mwfl::Command({103}, L"Undo"))
            .Add(mwfl::Command({104}, L"Redo"));
    mwfl::ApplyActiveDocumentCommandProjection(
        commands, {{101}, {102}, {103}, {104}}, projection);
    mwfl::TransferDocument(left, right, {22});

    mwfl::DocumentSession session;
    session.workspaces.push_back(mwfl::CaptureWorkspaceSession(left));
    session.workspaces.push_back(mwfl::CaptureWorkspaceSession(right));
    const auto encoded = mwfl::SerializeDocumentSession(session);
    const auto parsed = mwfl::ParseDocumentSession(encoded);
    (void)parsed;

    const std::array decision{
        mwfl::DocumentCloseDecision{{11}, mwfl::DocumentCloseChoice::discard}};
    const auto close = mwfl::BuildCoordinatedClosePlan(left, decision);
    mwfl::ExecuteCoordinatedClose(left, close, {});
}
