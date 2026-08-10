#pragma once

#include <windows.h>
#include <uiribbon.h>
#include <wrl/client.h>

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
    std::exception_ptr error{};
    explicit operator bool() const noexcept {
        return status == RibbonModelStatus::success;
    }
};

struct RibbonProjectionResult {
    RibbonModelStatus status = RibbonModelStatus::invalid_argument;
    std::vector<RibbonCommandProjection> commands{};
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

enum class RibbonHostStatus {
    success,
    unavailable,
    invalid_argument,
    wrong_thread,
    wrong_apartment,
    com_failure,
    invalid_state,
};

struct RibbonHostResult {
    RibbonHostStatus status = RibbonHostStatus::com_failure;
    HRESULT error = E_FAIL;
    explicit operator bool() const noexcept {
        return status == RibbonHostStatus::success;
    }
};

// Optional Windows Ribbon Framework bridge. The host borrows owner, model, and
// CommandSet, owns its IUIFramework/application callback references, and stays
// on the creating STA thread. LoadUI consumes a compiled Ribbon resource from
// the supplied module. GetFramework is a borrowed escape hatch.
class RibbonFrameworkHost final {
public:
    RibbonFrameworkHost() noexcept = default;
    ~RibbonFrameworkHost() noexcept;
    RibbonFrameworkHost(const RibbonFrameworkHost&) = delete;
    RibbonFrameworkHost& operator=(const RibbonFrameworkHost&) = delete;
    RibbonFrameworkHost(RibbonFrameworkHost&&) = delete;
    RibbonFrameworkHost& operator=(RibbonFrameworkHost&&) = delete;

    RibbonHostResult Create(HWND owner, RibbonCommandModel& model,
                            CommandSet& commands) noexcept;
    RibbonHostResult Load(HINSTANCE module, const wchar_t* resource) noexcept;
    RibbonHostResult SetModes(std::uint32_t modes) noexcept;
    RibbonHostResult Invalidate(RibbonCommandId command,
                                UI_INVALIDATIONS flags = UI_INVALIDATIONS_ALLPROPERTIES,
                                const PROPERTYKEY* key = nullptr) noexcept;
    RibbonHostResult InvalidateAll() noexcept;
    void Destroy() noexcept;

    bool IsCreated() const noexcept { return framework_ != nullptr; }
    UINT GetHeight() const noexcept { return height_; }
    HWND GetOwner() const noexcept { return owner_; }
    IUIFramework* GetFramework() const noexcept { return framework_.Get(); }
    std::exception_ptr TakeCallbackException() noexcept;

private:
    class Callback;
    RibbonHostResult CheckThread() const noexcept;
    void SetHeight(UINT value) noexcept { height_ = value; }

    HWND owner_ = nullptr;
    DWORD thread_id_ = 0;
    RibbonCommandModel* model_ = nullptr;
    CommandSet* commands_ = nullptr;
    Callback* callback_raw_ = nullptr;
    Microsoft::WRL::ComPtr<IUIApplication> application_;
    Microsoft::WRL::ComPtr<IUIFramework> framework_;
    UINT height_ = 0;
};

}  // namespace mwtl
