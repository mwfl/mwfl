#include <mwtl/mwtl.h>

#include <array>

void BuildDocumentWorkspaceFixture() {
    mwtl::DocumentWorkspaceModel left{{1}, 8};
    mwtl::DocumentWorkspaceModel right{{2}, 8};
    left.Add({{11}, L"One", L"C:\\Docs\\one.txt", true, true, false});
    left.Add({{22}, L"Two"});
    auto projection = mwtl::BuildActiveDocumentCommandProjection(left);
    mwtl::CommandSet commands;
    commands.Add(mwtl::Command({101}, L"Save"))
            .Add(mwtl::Command({102}, L"Close"))
            .Add(mwtl::Command({103}, L"Undo"))
            .Add(mwtl::Command({104}, L"Redo"));
    mwtl::ApplyActiveDocumentCommandProjection(
        commands, {{101}, {102}, {103}, {104}}, projection);
    mwtl::TransferDocument(left, right, {22});

    mwtl::DocumentSession session;
    session.workspaces.push_back(mwtl::CaptureWorkspaceSession(left));
    session.workspaces.push_back(mwtl::CaptureWorkspaceSession(right));
    const auto encoded = mwtl::SerializeDocumentSession(session);
    const auto parsed = mwtl::ParseDocumentSession(encoded);
    (void)parsed;

    const std::array decision{
        mwtl::DocumentCloseDecision{{11}, mwtl::DocumentCloseChoice::discard}};
    const auto close = mwtl::BuildCoordinatedClosePlan(left, decision);
    mwtl::ExecuteCoordinatedClose(left, close, {});
}
