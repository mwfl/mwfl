#include <mwfl/mwfl.h>

#include <array>

void BuildDockingWorkspaceFixture() {
    constexpr mwfl::DockGroupId documents{10};
    constexpr mwfl::DockGroupId tools{20};
    constexpr mwfl::DockPanelId editor{1};
    constexpr mwfl::DockPanelId output{2};

    mwfl::DockLayoutModel model{documents, {100},
                                mwfl::DockGroupRole::document};
    model.AddDockedGroup(tools, {200}, mwfl::DockGroupRole::tool,
                         documents, {300}, mwfl::DockEdge::bottom, 0.7);
    model.AddPanel({editor, L"main.cpp", mwfl::DockPanelRole::document},
                   documents);
    model.AddPanel({output, L"Output", mwfl::DockPanelRole::tool}, tools);

    const std::array targets{
        mwfl::DockDropTarget{1, mwfl::DockTargetKind::tab_group,
                             {0.0, 0.0, 400.0, 300.0}, tools},
        mwfl::DockDropTarget{2, mwfl::DockTargetKind::auto_hide,
                             {0.0, 300.0, 400.0, 40.0}, {},
                             mwfl::DockEdge::bottom}};
    mwfl::DockDragSession drag;
    drag.Begin(model, output);
    const auto target = drag.UpdateTarget({100.0, 100.0}, targets);
    if (target) {
        mwfl::DockMutation mutation;
        mutation.kind = mwfl::DockMutationKind::move_to_group;
        mutation.panel = output;
        mutation.target_group = target->group;
        if (const auto transaction = model.Propose(mutation))
            drag.SetProposal(*transaction);
    }
    drag.Cancel(mwfl::DockDragCancelReason::escape);

    const auto encoded = mwfl::SerializeDockingSession(model.GetSnapshot());
    const auto parsed = mwfl::ParseDockingSession(encoded);
    (void)parsed;
}
