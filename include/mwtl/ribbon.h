#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <mwtl/command.h>

namespace mwtl {

struct RibbonCommandId {
    std::uint32_t value = 0;
    constexpr auto operator<=>(const RibbonCommandId&) const noexcept = default;
    constexpr explicit operator bool() const noexcept { return value != 0; }
};

struct RibbonContextId {
    std::uint32_t value = 0;
    constexpr auto operator<=>(const RibbonContextId&) const noexcept = default;
};

struct RibbonCommandBinding {
    RibbonCommandId ribbon{};
    ControlId command{};
    std::uint32_t modes = 1;
    RibbonContextId context{};
};

struct RibbonRecentItem {
    std::wstring id;
    std::wstring label;
    std::wstring path;
};

struct RibbonCommandProjection {
    RibbonCommandId ribbon{};
    ControlId command{};
    std::wstring label;
    bool enabled = false;
    bool checked = false;
    bool visible = false;
    std::optional<int> image_index;
};

enum class RibbonModelStatus {
    success,
    invalid_argument,
    duplicate_id,
    not_found,
    command_missing,
    limit_exceeded,
    callback_failed,
};

struct RibbonModelResult {
    RibbonModelStatus status = RibbonModelStatus::invalid_argument;
    std::exception_ptr error;
    explicit operator bool() const noexcept {
        return status == RibbonModelStatus::success;
    }
};

struct RibbonProjectionResult {
    RibbonModelStatus status = RibbonModelStatus::invalid_argument;
    std::vector<RibbonCommandProjection> commands;
    explicit operator bool() const noexcept {
        return status == RibbonModelStatus::success;
    }
};

// Pointer-free Ribbon command/property state. This pure model does not own a
// CommandSet, COM interface, HWND, image, or generated Ribbon resource. IDs are
// stable application/resource identities. Mutations and projection use one UI
// thread when composed with CommandSet or the native Ribbon Framework host.
class RibbonCommandModel final {
public:
    static constexpr std::size_t MaximumBindings = 4096;
    static constexpr std::size_t MaximumRecentItems = 128;

    RibbonModelResult Add(RibbonCommandBinding binding) noexcept;
    RibbonModelResult Remove(RibbonCommandId ribbon) noexcept;
    RibbonModelResult SetActiveModes(std::uint32_t modes) noexcept;
    RibbonModelResult SetContextVisible(RibbonContextId context, bool visible) noexcept;
    RibbonModelResult SetRecentItems(std::span<const RibbonRecentItem> items) noexcept;
    RibbonProjectionResult Project(const CommandSet& commands) const noexcept;
    RibbonModelResult Execute(RibbonCommandId ribbon,
                              const CommandSet& commands) const noexcept;

    const RibbonCommandBinding* Find(RibbonCommandId ribbon) const noexcept;
    std::optional<RibbonCommandId> Find(ControlId command) const noexcept;
    std::span<const RibbonCommandBinding> GetBindings() const noexcept {
        return bindings_;
    }
    std::span<const RibbonRecentItem> GetRecentItems() const noexcept {
        return recent_items_;
    }
    std::uint32_t GetActiveModes() const noexcept { return active_modes_; }
    bool IsContextVisible(RibbonContextId context) const noexcept;

private:
    struct ContextState { RibbonContextId id{}; bool visible = false; };
    std::vector<RibbonCommandBinding> bindings_;
    std::vector<RibbonRecentItem> recent_items_;
    std::vector<ContextState> contexts_;
    std::uint32_t active_modes_ = 1;
};

}  // namespace mwtl
