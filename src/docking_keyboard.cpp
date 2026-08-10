#include <mwtl/docking_keyboard.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace mwtl {
namespace {

bool Usable(const DockDropTarget& target) noexcept {
    if (!target.enabled || target.id == 0 || !target.bounds.IsValid()) return false;
    return (target.kind != DockTargetKind::tab_group &&
            target.kind != DockTargetKind::split) || target.group;
}

DockPointDip Center(const DockDropTarget& target) noexcept {
    return {target.bounds.x + target.bounds.width / 2.0,
            target.bounds.y + target.bounds.height / 2.0};
}

}  // namespace

std::wstring DescribeDockTarget(const DockDropTarget& target) {
    std::wstring result;
    switch (target.kind) {
    case DockTargetKind::tab_group: result = L"Tab with group "; break;
    case DockTargetKind::split:
        switch (target.edge) {
        case DockEdge::left: result = L"Dock left of group "; break;
        case DockEdge::top: result = L"Dock above group "; break;
        case DockEdge::right: result = L"Dock right of group "; break;
        case DockEdge::bottom: result = L"Dock below group "; break;
        }
        break;
    case DockTargetKind::auto_hide:
        result = L"Auto-hide on ";
        switch (target.edge) {
        case DockEdge::left: result += L"left edge"; break;
        case DockEdge::top: result += L"top edge"; break;
        case DockEdge::right: result += L"right edge"; break;
        case DockEdge::bottom: result += L"bottom edge"; break;
        }
        return result;
    case DockTargetKind::floating: return L"Float panel";
    }
    result += std::to_wstring(target.group.value);
    return result;
}

bool DockKeyboardSession::Begin(
    DockPanelId panel, std::span<const DockDropTarget> targets,
    std::optional<std::uint64_t> preferred_target) {
    if (active_ || !panel || targets.empty() || targets.size() > 4096) return false;
    std::vector<DockDropTarget> accepted;
    accepted.reserve(targets.size());
    for (const auto& target : targets) {
        if (!Usable(target)) continue;
        if (std::any_of(accepted.begin(), accepted.end(),
            [&](const DockDropTarget& value) { return value.id == target.id; }))
            return false;
        accepted.push_back(target);
    }
    if (accepted.empty()) return false;
    targets_ = std::move(accepted);
    panel_ = panel;
    selected_ = 0;
    if (preferred_target) {
        const auto found = std::find_if(targets_.begin(), targets_.end(),
            [&](const DockDropTarget& target) { return target.id == *preferred_target; });
        if (found != targets_.end())
            selected_ = static_cast<std::size_t>(found - targets_.begin());
    }
    active_ = true;
    return true;
}

bool DockKeyboardSession::SelectIndex(std::size_t index) noexcept {
    if (!active_ || index >= targets_.size()) return false;
    selected_ = index;
    return true;
}

bool DockKeyboardSession::Move(DockKeyboardMove direction) noexcept {
    if (!active_ || !selected_ || targets_.empty()) return false;
    if (direction == DockKeyboardMove::next)
        return SelectIndex((*selected_ + 1) % targets_.size());
    if (direction == DockKeyboardMove::previous)
        return SelectIndex((*selected_ + targets_.size() - 1) % targets_.size());

    const DockPointDip current = Center(targets_[*selected_]);
    std::optional<std::size_t> best;
    double best_score = (std::numeric_limits<double>::max)();
    for (std::size_t i = 0; i < targets_.size(); ++i) {
        if (i == *selected_) continue;
        const DockPointDip candidate = Center(targets_[i]);
        const double dx = candidate.x - current.x;
        const double dy = candidate.y - current.y;
        double primary = 0.0, secondary = 0.0;
        switch (direction) {
        case DockKeyboardMove::left: primary = -dx; secondary = std::abs(dy); break;
        case DockKeyboardMove::right: primary = dx; secondary = std::abs(dy); break;
        case DockKeyboardMove::up: primary = -dy; secondary = std::abs(dx); break;
        case DockKeyboardMove::down: primary = dy; secondary = std::abs(dx); break;
        default: break;
        }
        if (primary <= 0.0) continue;
        const double score = primary * 1024.0 + secondary;
        if (score < best_score ||
            (best && score == best_score && targets_[i].id < targets_[*best].id)) {
            best = i;
            best_score = score;
        }
    }
    return best ? SelectIndex(*best) : false;
}

std::optional<DockKeyboardSelection> DockKeyboardSession::Accept() noexcept {
    if (!active_ || !selected_) return std::nullopt;
    try {
        DockKeyboardSelection result{targets_[*selected_],
                                     DescribeDockTarget(targets_[*selected_])};
        active_ = false;
        return result;
    } catch (...) {
        return std::nullopt;
    }
}

void DockKeyboardSession::Cancel() noexcept {
    active_ = false;
    selected_.reset();
}

void DockKeyboardSession::Reset() noexcept {
    panel_ = {};
    targets_.clear();
    selected_.reset();
    active_ = false;
}

std::optional<DockKeyboardSelection> DockKeyboardSession::GetSelection() const {
    if (!active_ || !selected_) return std::nullopt;
    return DockKeyboardSelection{targets_[*selected_],
                                 DescribeDockTarget(targets_[*selected_])};
}

}  // namespace mwtl
