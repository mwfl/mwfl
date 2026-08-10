#include <mwtl/mwtl.h>

#include <array>

void BuildDockingWorkspaceFixture() {
    constexpr mwtl::DockGroupId documents{10};
    constexpr mwtl::DockGroupId tools{20};
    constexpr mwtl::DockPanelId editor{1};
    constexpr mwtl::DockPanelId output{2};

    mwtl::DockLayoutModel model{documents, {100},
                                mwtl::DockGroupRole::document};
    model.AddDockedGroup(tools, {200}, mwtl::DockGroupRole::tool,
                         documents, {300}, mwtl::DockEdge::bottom, 0.7);
    model.AddPanel({editor, L"main.cpp", mwtl::DockPanelRole::document},
                   documents);
    model.AddPanel({output, L"Output", mwtl::DockPanelRole::tool}, tools);

    const std::array targets{
        mwtl::DockDropTarget{1, mwtl::DockTargetKind::tab_group,
                             {0.0, 0.0, 400.0, 300.0}, tools},
        mwtl::DockDropTarget{2, mwtl::DockTargetKind::auto_hide,
                             {0.0, 300.0, 400.0, 40.0}, {},
                             mwtl::DockEdge::bottom}};
    mwtl::DockDragSession drag;
    drag.Begin(model, output);
    const auto target = drag.UpdateTarget({100.0, 100.0}, targets);
    if (target) {
        mwtl::DockMutation mutation;
        mutation.kind = mwtl::DockMutationKind::move_to_group;
        mutation.panel = output;
        mutation.target_group = target->group;
        if (const auto transaction = model.Propose(mutation))
            drag.SetProposal(*transaction);
    }
    drag.Cancel(mwtl::DockDragCancelReason::escape);

    const auto encoded = mwtl::SerializeDockingSession(model.GetSnapshot());
    const auto parsed = mwtl::ParseDockingSession(encoded);
    (void)parsed;
}
