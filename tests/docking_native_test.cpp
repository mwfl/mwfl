#include <mwtl/docking_native.h>

namespace {

HWND MakeTopLevel(int x) {
    return ::CreateWindowExW(0, L"STATIC", L"dock host", WS_OVERLAPPEDWINDOW,
        x, 20, 500, 350, nullptr, nullptr, ::GetModuleHandleW(nullptr), nullptr);
}

HWND MakeChild(HWND parent, int id, const wchar_t* title = L"") {
    return ::CreateWindowExW(WS_EX_CONTROLPARENT, L"STATIC", title,
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, 0, 0, 300, 240, parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        ::GetModuleHandleW(nullptr), nullptr);
}

}  // namespace

int main() {
    using namespace mwtl;
    const HWND main_host = MakeTopLevel(20);
    const HWND floating_host = MakeTopLevel(550);
    if (!main_host || !floating_host) return 1;
    ::ShowWindow(main_host, SW_SHOWNOACTIVATE);
    ::ShowWindow(floating_host, SW_SHOWNOACTIVATE);
    const HWND group_one = MakeChild(main_host, 101, L"group one");
    const HWND group_two = MakeChild(floating_host, 102, L"group two");
    const HWND first = ::CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"first",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 200, 120, group_one,
        reinterpret_cast<HMENU>(201), ::GetModuleHandleW(nullptr), nullptr);
    const HWND second = ::CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"second",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 200, 120, group_one,
        reinterpret_cast<HMENU>(202), ::GetModuleHandleW(nullptr), nullptr);
    if (!group_one || !group_two || !first || !second) return 2;

    DockLayoutModel model{{10}, {100}, DockGroupRole::tool};
    if (!model.AddDockedGroup({20}, {200}, DockGroupRole::tool, {10}, {300},
                              DockEdge::right) ||
        !model.AddPanel({{1}, L"First", DockPanelRole::tool}, {10}) ||
        !model.AddPanel({{2}, L"Second", DockPanelRole::tool}, {10})) return 3;

    DockNativeWorkspaceAdapter adapter;
    if (!adapter.Attach(main_host) || adapter.Attach(main_host).status !=
            DockNativeStatus::invalid_argument ||
        !adapter.BindGroup({10}, group_one) || !adapter.BindGroup({20}, group_two) ||
        !adapter.BindPanel({1}, first) || !adapter.BindPanel({2}, second)) return 4;
    if (adapter.BindPanel({3}, first).status != DockNativeStatus::duplicate_binding ||
        adapter.BindGroup({30}, group_one).status != DockNativeStatus::duplicate_binding)
        return 5;
    const auto synchronized = adapter.Synchronize(model.GetSnapshot());
    if (!synchronized) return 6;
    if (::GetParent(first) != group_one || ::GetParent(second) != group_one ||
        ::IsWindowVisible(first) != FALSE || ::IsWindowVisible(second) == FALSE)
        return 6;

    ::SetFocus(second);
    DockMutation move;
    move.kind = DockMutationKind::move_to_group;
    move.panel = {1};
    move.target_group = {20};
    const auto transaction = model.Propose(move);
    DockNativeStatus failure{};
    auto plan = transaction ? adapter.Prepare(transaction->proposed, &failure) : std::nullopt;
    if (!transaction || !plan || failure != DockNativeStatus::success ||
        !plan->IsValid() || !adapter.Adopt(*plan) || ::GetParent(first) != group_two ||
        ::IsWindowVisible(first) == FALSE || ::GetFocus() != first) return 7;
    if (!adapter.Rollback(*plan) || ::GetParent(first) != group_one ||
        ::GetFocus() != second || ::IsWindowVisible(first) != FALSE) return 8;
    if (!adapter.Adopt(*plan) || !model.Commit(*transaction) || !adapter.Commit(*plan) ||
        ::GetParent(first) != group_two || model.FindPanelGroup({1}) != DockGroupId{20} ||
        adapter.Rollback(*plan).status != DockNativeStatus::invalid_state) return 9;

    DockMutation hide;
    hide.kind = DockMutationKind::auto_hide;
    hide.panel = {2};
    hide.edge = DockEdge::left;
    const auto hidden = model.Propose(hide);
    if (!hidden) return 10;
    auto hidden_plan = adapter.Prepare(hidden->proposed);
    if (!hidden_plan || !adapter.Adopt(*hidden_plan) ||
        ::GetParent(second) != main_host || ::IsWindowVisible(second) != FALSE ||
        !model.Commit(*hidden) || !adapter.Commit(*hidden_plan)) return 11;

    DockMutation close;
    close.kind = DockMutationKind::close_panel;
    close.panel = {1};
    const auto closed = model.Propose(close);
    auto closed_plan = closed ? adapter.Prepare(closed->proposed) : std::nullopt;
    if (!closed || !closed_plan || !adapter.Adopt(*closed_plan) ||
        ::GetParent(first) != main_host || ::IsWindowVisible(first) != FALSE)
        return 12;
    if (!adapter.Rollback(*closed_plan) || ::GetParent(first) != group_two ||
        ::IsWindowVisible(first) == FALSE) return 13;
    if (!adapter.Adopt(*closed_plan) || !model.Commit(*closed) ||
        !adapter.Commit(*closed_plan) || model.FindPanel({1}) ||
        ::GetParent(first) != main_host || ::IsWindowVisible(first) != FALSE)
        return 14;

    auto stale_snapshot = model.GetSnapshot();
    ::DestroyWindow(first);
    if (adapter.Prepare(stale_snapshot, &failure) ||
        failure != DockNativeStatus::stale_window) return 15;
    if (!adapter.UnbindPanel({1}) ||
        adapter.UnbindPanel({1}).status != DockNativeStatus::not_found ||
        !adapter.UnbindGroup({20})) return 16;

    adapter.Detach();
    if (adapter.GetParkingParent() || adapter.FindPanel({2}) || adapter.FindGroup({10}))
        return 17;
    if (!::IsWindow(second) || ::GetParent(second) != main_host) return 18;
    ::DestroyWindow(main_host);
    ::DestroyWindow(floating_host);
    return 0;
}
