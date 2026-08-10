#include <mwfl/mwfl.h>

#include <windows.h>

#include <array>
#include <cstdint>

namespace {

using namespace mwfl;

struct GuiResources {
    DWORD user = 0;
    DWORD gdi = 0;
};

GuiResources Resources() noexcept {
    const HANDLE process = ::GetCurrentProcess();
    return {::GetGuiResources(process, GR_USEROBJECTS),
            ::GetGuiResources(process, GR_GDIOBJECTS)};
}

DockLayoutModel MakeModel() {
    DockLayoutModel model{{10}, {100}, DockGroupRole::document};
    model.AddDockedGroup({20}, {200}, DockGroupRole::tool, {10}, {300},
                         DockEdge::left, 0.25);
    model.AddDockedGroup({30}, {201}, DockGroupRole::tool, {10}, {301},
                         DockEdge::bottom, 0.70);
    model.AddPanel({{1}, L"main.cpp", DockPanelRole::document}, {10});
    model.AddPanel({{2}, L"Explorer", DockPanelRole::tool}, {20});
    model.AddPanel({{3}, L"Output", DockPanelRole::tool}, {30});
    return model;
}

bool Move(DockLayoutModel& model, DockNativeWorkspaceAdapter& native,
          DockPanelId panel, DockGroupId destination) {
    DockMutation mutation;
    mutation.kind = DockMutationKind::move_to_group;
    mutation.panel = panel;
    mutation.target_group = destination;
    const auto transaction = model.Propose(mutation);
    if (!transaction) return false;
    auto adoption = native.Prepare(transaction->proposed);
    if (!adoption || !native.Adopt(*adoption)) return false;
    if (!model.Commit(*transaction)) {
        native.Rollback(*adoption);
        return false;
    }
    return static_cast<bool>(native.Commit(*adoption));
}

}  // namespace

int main() {
    HWND owner = ::CreateWindowExW(0, L"STATIC", L"docking stress owner",
        WS_OVERLAPPEDWINDOW, 0, 0, 800, 600, nullptr, nullptr,
        ::GetModuleHandleW(nullptr), nullptr);
    HWND documents = ::CreateWindowExW(0, L"STATIC", L"documents",
        WS_CHILD | WS_VISIBLE, 0, 0, 400, 300, owner, nullptr,
        ::GetModuleHandleW(nullptr), nullptr);
    HWND left = ::CreateWindowExW(0, L"STATIC", L"left",
        WS_CHILD | WS_VISIBLE, 0, 0, 200, 300, owner, nullptr,
        ::GetModuleHandleW(nullptr), nullptr);
    HWND bottom = ::CreateWindowExW(0, L"STATIC", L"bottom",
        WS_CHILD | WS_VISIBLE, 0, 300, 600, 200, owner, nullptr,
        ::GetModuleHandleW(nullptr), nullptr);
    HWND document = ::CreateWindowExW(0, L"EDIT", L"document",
        WS_CHILD | WS_VISIBLE, 0, 0, 100, 100, owner, nullptr,
        ::GetModuleHandleW(nullptr), nullptr);
    HWND explorer = ::CreateWindowExW(0, L"LISTBOX", L"explorer",
        WS_CHILD | WS_VISIBLE, 0, 0, 100, 100, owner, nullptr,
        ::GetModuleHandleW(nullptr), nullptr);
    HWND output = ::CreateWindowExW(0, L"LISTBOX", L"output",
        WS_CHILD | WS_VISIBLE, 0, 0, 100, 100, owner, nullptr,
        ::GetModuleHandleW(nullptr), nullptr);
    if (!owner || !documents || !left || !bottom || !document || !explorer || !output)
        return 1;

    auto model = MakeModel();
    DockNativeWorkspaceAdapter native;
    if (!native.Attach(owner) || !native.BindGroup({10}, documents) ||
        !native.BindGroup({20}, left) || !native.BindGroup({30}, bottom) ||
        !native.BindPanel({1}, document) || !native.BindPanel({2}, explorer) ||
        !native.BindPanel({3}, output) || !native.Synchronize(model.GetSnapshot()))
        return 2;

    // Warm up native classes and one floating-host lifecycle before measuring.
    {
        DockFloatingWindow warmup;
        DockFloatingWindowOptions options;
        options.placement = {30, 30, 280, 180, L"primary"};
        if (!warmup.Create(owner, options)) return 3;
        warmup.Destroy();
    }
    const GuiResources before = Resources();

    const std::array targets{
        DockDropTarget{1, DockTargetKind::tab_group, {0, 0, 200, 200}, {20}},
        DockDropTarget{2, DockTargetKind::tab_group, {200, 0, 200, 200}, {30}}};
    for (int iteration = 0; iteration < 200; ++iteration) {
        const DockGroupId destination = (iteration % 2 == 0) ? DockGroupId{20}
                                                              : DockGroupId{30};
        if (!Move(model, native, {3}, destination) ||
            model.FindPanelGroup({3}) != destination) return 4;

        DockMutation hide;
        hide.kind = DockMutationKind::auto_hide;
        hide.panel = {2};
        hide.edge = DockEdge::left;
        const auto hidden = model.Propose(hide);
        if (!hidden || !model.Commit(*hidden) ||
            !native.Synchronize(model.GetSnapshot())) return 5;
        if (!Move(model, native, {2}, {20})) return 6;

        DockDragSession drag;
        if (!drag.Begin(model, {3}) || !drag.UpdateTarget({50, 50}, targets))
            return 7;
        drag.Cancel(iteration % 2 == 0 ? DockDragCancelReason::escape
                                      : DockDragCancelReason::capture_lost);
        if (drag.GetState() != DockDragState::cancelled) return 8;

        const auto encoded = SerializeDockingSession(model.GetSnapshot());
        const auto parsed = ParseDockingSession(encoded);
        if (!parsed || parsed.snapshot->panels.size() != 3) return 9;

        DockFloatingWindow floating;
        DockFloatingWindowOptions options;
        options.placement = {40, 40, 300, 190, L"primary"};
        if (!floating.Create(owner, options)) return 10;
        floating.Show(false);
        floating.Hide();
        floating.Destroy();
        floating.Destroy();
    }

    const GuiResources after = Resources();
    if (after.user > before.user + 4 || after.gdi > before.gdi + 4) return 11;

    // A panel destroyed between prepare and adoption is rejected, not adopted
    // from stale state, and teardown remains repeatable.
    DockMutation stale_move;
    stale_move.kind = DockMutationKind::move_to_group;
    stale_move.panel = {3};
    stale_move.target_group = model.FindPanelGroup({3}) == DockGroupId{20}
        ? DockGroupId{30} : DockGroupId{20};
    const auto stale_transaction = model.Propose(stale_move);
    auto stale_adoption = stale_transaction
        ? native.Prepare(stale_transaction->proposed) : std::nullopt;
    if (!stale_adoption || !::DestroyWindow(output) ||
        native.Adopt(*stale_adoption).status == DockNativeStatus::success ||
        model.GetRevision() != stale_transaction->source_revision)
        return 12;

    native.Detach();
    native.Detach();
    ::DestroyWindow(owner);
    return 0;
}
