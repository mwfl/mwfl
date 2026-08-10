#include <mwfl/layout.h>

#include <algorithm>
#include <climits>
#include <cmath>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace mwfl {
namespace {

constexpr float kEpsilon = 0.0001f;

float ClampFinite(float value, float minimum, float maximum) noexcept {
    minimum = (std::max)(0.0f, minimum);
    maximum = (std::max)(minimum, maximum);
    return (std::clamp)(value, minimum, maximum);
}

float Main(SizeDip size, LayoutDirection direction) noexcept {
    return direction == LayoutDirection::row
        ? size.width.value
        : size.height.value;
}

float Cross(SizeDip size, LayoutDirection direction) noexcept {
    return direction == LayoutDirection::row
        ? size.height.value
        : size.width.value;
}

SizeDip MakeSize(
    float main,
    float cross,
    LayoutDirection direction) noexcept {
    return direction == LayoutDirection::row
        ? SizeDip{Dip{main}, Dip{cross}}
        : SizeDip{Dip{cross}, Dip{main}};
}

SizeDip MeasureWindow(HWND window, DpiContext dpi) noexcept {
    RECT bounds{};
    if (window == nullptr || ::GetWindowRect(window, &bounds) == FALSE) {
        return {};
    }
    return {
        dpi.FromPixels(bounds.right - bounds.left),
        dpi.FromPixels(bounds.bottom - bounds.top),
    };
}

}  // namespace

struct LayoutNode::Placement {
    HWND window = nullptr;
    RectDip bounds{};
};

struct LayoutNode::Child {
    HWND window = nullptr;
    std::unique_ptr<LayoutNode> node;
    LayoutLength length{};
    LayoutItemOptions options{};
    std::function<SizeDip(DpiContext)> measure;

    Child(HWND input_window, LayoutLength input_length,
          LayoutItemOptions input_options,
          std::function<SizeDip(DpiContext)> input_measure = {})
        : window(input_window),
          length(input_length),
          options(std::move(input_options)),
          measure(std::move(input_measure)) {}

    Child(LayoutNode input_node, LayoutLength input_length,
          LayoutItemOptions input_options)
        : node(std::make_unique<LayoutNode>(std::move(input_node))),
          length(input_length),
          options(std::move(input_options)) {}

    Child(Child&&) noexcept = default;
    Child& operator=(Child&&) noexcept = default;
    Child(const Child&) = delete;
    Child& operator=(const Child&) = delete;
};

LayoutNode::LayoutNode(LayoutDirection direction) noexcept
    : direction_(direction) {}
LayoutNode::~LayoutNode() noexcept = default;
LayoutNode::LayoutNode(LayoutNode&&) noexcept = default;
LayoutNode& LayoutNode::operator=(LayoutNode&&) noexcept = default;

LayoutNode& LayoutNode::Gap(Dip value) & noexcept {
    gap_ = Dip{(std::max)(0.0f, value.value)};
    return *this;
}

LayoutNode& LayoutNode::Margin(Thickness value) & noexcept {
    margin_ = {
        Dip{(std::max)(0.0f, value.left.value)},
        Dip{(std::max)(0.0f, value.top.value)},
        Dip{(std::max)(0.0f, value.right.value)},
        Dip{(std::max)(0.0f, value.bottom.value)},
    };
    return *this;
}

LayoutNode& LayoutNode::Margin(Dip uniform) & noexcept {
    const Dip value{(std::max)(0.0f, uniform.value)};
    return Margin({value, value, value, value});
}

LayoutNode& LayoutNode::Add(
    HWND window,
    LayoutLength length,
    LayoutItemOptions options) & {
    if (window == nullptr) {
        throw std::invalid_argument("mwfl layout window is null");
    }
    children_.emplace_back(window, length, std::move(options));
    return *this;
}

LayoutNode& LayoutNode::AddMeasured(
    HWND window,
    std::function<SizeDip(DpiContext)> measure,
    LayoutLength length,
    LayoutItemOptions options) {
    if (window == nullptr) {
        throw std::invalid_argument("mwfl layout window is null");
    }
    children_.emplace_back(
        window, length, std::move(options), std::move(measure));
    return *this;
}

LayoutNode& LayoutNode::Add(
    LayoutNode child,
    LayoutLength length,
    LayoutItemOptions options) & {
    children_.emplace_back(
        std::move(child), length, std::move(options));
    return *this;
}

LayoutDirection LayoutNode::GetDirection() const noexcept { return direction_; }
std::size_t LayoutNode::GetCount() const noexcept { return children_.size(); }

SizeDip LayoutNode::Measure(DpiContext dpi) const {
    if (direction_ == LayoutDirection::overlay) {
        float width = 0.0f;
        float height = 0.0f;
        for (const Child& child : children_) {
            const SizeDip desired = child.options.preferred_size.value_or(
                child.node ? child.node->Measure(dpi)
                           : child.measure ? child.measure(dpi)
                                           : MeasureWindow(child.window, dpi));
            width = (std::max)(width, desired.width.value);
            height = (std::max)(height, desired.height.value);
        }
        return {
            Dip{width + margin_.left.value + margin_.right.value},
            Dip{height + margin_.top.value + margin_.bottom.value},
        };
    }
    const float margin_main = direction_ == LayoutDirection::row
        ? margin_.left.value + margin_.right.value
        : margin_.top.value + margin_.bottom.value;
    const float margin_cross = direction_ == LayoutDirection::row
        ? margin_.top.value + margin_.bottom.value
        : margin_.left.value + margin_.right.value;

    float main = margin_main;
    float cross = 0.0f;
    for (const Child& child : children_) {
        SizeDip desired = child.options.preferred_size.value_or(
            child.node ? child.node->Measure(dpi)
                       : child.measure ? child.measure(dpi)
                                       : MeasureWindow(child.window, dpi));
        float child_main = Main(desired, direction_);
        if (child.length.kind == LayoutLengthKind::fixed) {
            child_main = child.length.value.value;
        } else if (child.length.kind == LayoutLengthKind::stretch) {
            child_main = (std::max)(child_main, child.length.minimum.value);
        }
        child_main = ClampFinite(
            child_main,
            child.length.minimum.value,
            child.length.maximum.value);
        const float child_cross = ClampFinite(
            Cross(desired, direction_),
            child.options.minimum_cross.value,
            child.options.maximum_cross.value);
        main += child_main;
        cross = (std::max)(cross, child_cross);
    }
    if (children_.size() > 1) {
        main += gap_.value * static_cast<float>(children_.size() - 1);
    }
    return MakeSize(main, cross + margin_cross, direction_);
}

void LayoutNode::CollectPlacements(
    RectDip bounds,
    DpiContext dpi,
    std::vector<Placement>& output) const {
    const float left = bounds.origin.x.value + margin_.left.value;
    const float top = bounds.origin.y.value + margin_.top.value;
    const float inner_width = (std::max)(
        0.0f, bounds.size.width.value - margin_.left.value - margin_.right.value);
    const float inner_height = (std::max)(
        0.0f, bounds.size.height.value - margin_.top.value - margin_.bottom.value);
    if (direction_ == LayoutDirection::overlay) {
        const RectDip child_bounds{
            Dip{left}, Dip{top}, Dip{inner_width}, Dip{inner_height}};
        for (const Child& child : children_) {
            if (child.node) {
                child.node->CollectPlacements(child_bounds, dpi, output);
            } else if (child.window != nullptr) {
                output.push_back({child.window, child_bounds});
            }
        }
        return;
    }
    const float available_main = direction_ == LayoutDirection::row
        ? inner_width : inner_height;
    const float available_cross = direction_ == LayoutDirection::row
        ? inner_height : inner_width;
    const float gaps = children_.size() > 1
        ? gap_.value * static_cast<float>(children_.size() - 1)
        : 0.0f;

    std::vector<float> allocated;
    allocated.reserve(children_.size());
    float total = gaps;
    float stretch_weight = 0.0f;
    for (const Child& child : children_) {
        const SizeDip measured = child.options.preferred_size.value_or(
            child.node ? child.node->Measure(dpi)
                       : child.measure ? child.measure(dpi)
                                       : MeasureWindow(child.window, dpi));
        const float measured_main = Main(measured, direction_);
        float value = measured_main;
        if (child.length.kind == LayoutLengthKind::fixed) {
            value = child.length.value.value;
        } else if (child.length.kind == LayoutLengthKind::stretch) {
            value = child.length.minimum.value;
            stretch_weight += (std::max)(0.0f, child.length.weight);
        }
        value = ClampFinite(
            value,
            child.length.minimum.value,
            child.length.maximum.value);
        allocated.push_back(value);
        total += value;
    }

    float remaining = available_main - total;
    if (remaining > kEpsilon && stretch_weight > kEpsilon) {
        std::vector<bool> open(children_.size(), true);
        while (remaining > kEpsilon) {
            float weight = 0.0f;
            for (std::size_t i = 0; i < children_.size(); ++i) {
                if (open[i] && children_[i].length.kind == LayoutLengthKind::stretch) {
                    weight += (std::max)(0.0f, children_[i].length.weight);
                }
            }
            if (weight <= kEpsilon) break;
            float distributed = 0.0f;
            for (std::size_t i = 0; i < children_.size(); ++i) {
                const Child& child = children_[i];
                if (!open[i] || child.length.kind != LayoutLengthKind::stretch) continue;
                const float share = remaining *
                    ((std::max)(0.0f, child.length.weight) / weight);
                const float maximum = child.length.maximum.value;
                const float added = (std::min)(share, maximum - allocated[i]);
                allocated[i] += (std::max)(0.0f, added);
                distributed += (std::max)(0.0f, added);
                if (allocated[i] + kEpsilon >= maximum) open[i] = false;
            }
            if (distributed <= kEpsilon) break;
            remaining -= distributed;
        }
    } else if (remaining < -kEpsilon) {
        float deficit = -remaining;
        float shrinkable = 0.0f;
        for (std::size_t i = 0; i < children_.size(); ++i) {
            if (children_[i].length.kind == LayoutLengthKind::auto_size) {
                shrinkable += allocated[i] - children_[i].length.minimum.value;
            }
        }
        if (shrinkable > kEpsilon) {
            const float ratio = (std::min)(1.0f, deficit / shrinkable);
            for (std::size_t i = 0; i < children_.size(); ++i) {
                if (children_[i].length.kind != LayoutLengthKind::auto_size) continue;
                const float capacity =
                    allocated[i] - children_[i].length.minimum.value;
                allocated[i] -= capacity * ratio;
            }
        }
    }

    float cursor = direction_ == LayoutDirection::row ? left : top;
    for (std::size_t i = 0; i < children_.size(); ++i) {
        const Child& child = children_[i];
        const SizeDip measured = child.options.preferred_size.value_or(
            child.node ? child.node->Measure(dpi)
                       : child.measure ? child.measure(dpi)
                                       : MeasureWindow(child.window, dpi));
        float cross_size = child.options.alignment == CrossAlignment::stretch
            ? available_cross
            : Cross(measured, direction_);
        cross_size = ClampFinite(
            cross_size,
            child.options.minimum_cross.value,
            child.options.maximum_cross.value);
        float cross_offset = 0.0f;
        if (child.options.alignment == CrossAlignment::center) {
            cross_offset = (available_cross - cross_size) / 2.0f;
        } else if (child.options.alignment == CrossAlignment::end) {
            cross_offset = available_cross - cross_size;
        }
        cross_offset = (std::max)(0.0f, cross_offset);

        RectDip child_bounds;
        if (direction_ == LayoutDirection::row) {
            child_bounds = {
                Dip{cursor}, Dip{top + cross_offset},
                Dip{allocated[i]}, Dip{cross_size}};
        } else {
            child_bounds = {
                Dip{left + cross_offset}, Dip{cursor},
                Dip{cross_size}, Dip{allocated[i]}};
        }
        if (child.node) {
            child.node->CollectPlacements(child_bounds, dpi, output);
        } else if (child.window != nullptr) {
            RectDip native_bounds = child_bounds;
            if (child.options.native_size) {
                if (child.options.native_size->width.value > 0.0f) {
                    native_bounds.size.width = child.options.native_size->width;
                }
                if (child.options.native_size->height.value > 0.0f) {
                    native_bounds.size.height = child.options.native_size->height;
                }
            }
            output.push_back({child.window, native_bounds});
        }
        cursor += allocated[i] + gap_.value;
    }
}

std::size_t LayoutNode::CountWindows() const noexcept {
    std::size_t count = 0;
    for (const Child& child : children_) {
        count += child.node ? child.node->CountWindows()
                            : (child.window != nullptr ? 1u : 0u);
    }
    return count;
}

LayoutNode Row() noexcept { return LayoutNode(LayoutDirection::row); }
LayoutNode Column() noexcept { return LayoutNode(LayoutDirection::column); }
LayoutNode Overlay() noexcept { return LayoutNode(LayoutDirection::overlay); }

LayoutHost::LayoutHost(LayoutNode root) noexcept
    : root_(std::move(root)) {}

void LayoutHost::SetRoot(LayoutNode root) noexcept {
    root_.emplace(std::move(root));
}

bool LayoutHost::HasRoot() const noexcept { return root_.has_value(); }

SizeDip LayoutHost::GetPreferredSize(HWND parent) const {
    if (!root_) return {};
    return root_->Measure(DpiContext::FromWindow(parent));
}

bool LayoutHost::Arrange(HWND parent) const {
    if (!root_ || parent == nullptr || ::IsWindow(parent) == FALSE) return false;
    RECT client{};
    if (::GetClientRect(parent, &client) == FALSE) return false;
    const DpiContext dpi = DpiContext::FromWindow(parent);
    const RectDip bounds{
        0.0_dip, 0.0_dip,
        dpi.FromPixels(client.right - client.left),
        dpi.FromPixels(client.bottom - client.top)};
    std::vector<LayoutNode::Placement> placements;
    placements.reserve(root_->CountWindows());
    root_->CollectPlacements(bounds, dpi, placements);

    std::unordered_set<HWND> unique_windows;
    unique_windows.reserve(placements.size());
    for (const auto& placement : placements) {
        if (::IsWindow(placement.window) == FALSE ||
            ::IsChild(parent, placement.window) == FALSE ||
            !unique_windows.insert(placement.window).second) {
            return false;
        }
    }
    if (placements.empty()) return true;
    if (placements.size() > static_cast<std::size_t>(INT_MAX)) return false;

    const auto native_bounds = [&](const LayoutNode::Placement& placement) {
        RECT pixels = dpi.ToPixels(placement.bounds);
        const HWND native_parent = ::GetParent(placement.window);
        if (native_parent != parent) {
            POINT origin{pixels.left, pixels.top};
            ::MapWindowPoints(parent, native_parent, &origin, 1);
            const int width = pixels.right - pixels.left;
            const int height = pixels.bottom - pixels.top;
            pixels = {origin.x, origin.y, origin.x + width, origin.y + height};
        }
        return pixels;
    };

    HDWP deferred = ::BeginDeferWindowPos(static_cast<int>(placements.size()));
    if (deferred != nullptr) {
        for (const auto& placement : placements) {
            const RECT pixels = native_bounds(placement);
            deferred = ::DeferWindowPos(
                deferred, placement.window, nullptr,
                pixels.left, pixels.top,
                pixels.right - pixels.left, pixels.bottom - pixels.top,
                SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER);
            if (deferred == nullptr) break;
        }
        if (deferred != nullptr && ::EndDeferWindowPos(deferred) != FALSE) {
            return true;
        }
    }

    bool succeeded = true;
    for (const auto& placement : placements) {
        const RECT pixels = native_bounds(placement);
        if (::SetWindowPos(
                placement.window, nullptr,
                pixels.left, pixels.top,
                pixels.right - pixels.left, pixels.bottom - pixels.top,
                SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER) == FALSE) {
            succeeded = false;
        }
    }
    return succeeded;
}

}  // namespace mwfl
