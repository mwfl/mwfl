#include <mwfl/docking_keyboard.h>

#include <array>

int main() {
    using namespace mwfl;
    const std::array targets{
        DockDropTarget{1, DockTargetKind::split, {0, 40, 40, 40}, {10},
                       DockEdge::left},
        DockDropTarget{2, DockTargetKind::tab_group, {100, 40, 40, 40}, {10}},
        DockDropTarget{3, DockTargetKind::split, {200, 40, 40, 40}, {10},
                       DockEdge::right},
        DockDropTarget{4, DockTargetKind::split, {100, 140, 40, 40}, {10},
                       DockEdge::bottom},
        DockDropTarget{5, DockTargetKind::auto_hide, {100, 240, 40, 40}, {},
                       DockEdge::bottom},
        DockDropTarget{6, DockTargetKind::floating, {300, 40, 40, 40}},
        DockDropTarget{7, DockTargetKind::floating, {400, 40, 0, 40}},
        DockDropTarget{8, DockTargetKind::floating, {500, 40, 40, 40}, {},
                       DockEdge::left, 0, false},
    };
    DockKeyboardSession session;
    if (!session.Begin({9}, targets, 2) || !session.IsActive() ||
        session.GetSelection()->target.id != 2 ||
        session.GetSelection()->announcement != L"Tab with group 10") return 1;
    if (!session.Move(DockKeyboardMove::left) ||
        session.GetSelection()->target.id != 1 ||
        session.GetSelection()->announcement != L"Dock left of group 10") return 2;
    if (!session.Move(DockKeyboardMove::right) ||
        session.GetSelection()->target.id != 2 ||
        !session.Move(DockKeyboardMove::down) ||
        session.GetSelection()->target.id != 4 ||
        !session.Move(DockKeyboardMove::down) ||
        session.GetSelection()->target.id != 5 ||
        session.GetSelection()->announcement != L"Auto-hide on bottom edge") return 3;
    if (!session.Move(DockKeyboardMove::up) ||
        session.GetSelection()->target.id != 4) return 4;
    if (!session.Move(DockKeyboardMove::next) ||
        session.GetSelection()->target.id != 5 ||
        !session.Move(DockKeyboardMove::previous) ||
        session.GetSelection()->target.id != 4) return 5;
    const auto accepted = session.Accept();
    if (!accepted || accepted->target.id != 4 || session.IsActive() ||
        session.Accept()) return 6;

    session.Reset();
    if (session.GetPanel() || session.GetSelection()) return 7;
    if (!session.Begin({10}, targets) || session.GetSelection()->target.id != 1)
        return 8;
    session.Cancel();
    if (session.IsActive() || session.GetSelection() ||
        session.Move(DockKeyboardMove::next)) return 9;

    auto duplicate = targets;
    duplicate[1].id = 1;
    session.Reset();
    if (session.Begin({1}, duplicate)) return 10;
    const std::array unusable{
        DockDropTarget{0, DockTargetKind::floating, {0, 0, 20, 20}},
        DockDropTarget{2, DockTargetKind::tab_group, {0, 0, 20, 20}, {}},
    };
    if (session.Begin({1}, unusable) || session.Begin({}, targets)) return 11;
    if (DescribeDockTarget({1, DockTargetKind::floating}) != L"Float panel" ||
        DescribeDockTarget({1, DockTargetKind::auto_hide, {}, {},
                            DockEdge::right}) != L"Auto-hide on right edge") return 12;
    return 0;
}
