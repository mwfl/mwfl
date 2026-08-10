#pragma once

#include <windows.h>

#include <compare>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <mwtl/command.h>

namespace mwtl {

struct MdiChildId {
    std::uint64_t value = 0;
    constexpr auto operator<=>(const MdiChildId&) const noexcept = default;
    constexpr explicit operator bool() const noexcept { return value != 0; }
};

struct MdiChildEntry {
    MdiChildId id{};
    std::wstring title;
    bool dirty = false;
    bool closable = true;
};

enum class MdiModelStatus {
    success,
    invalid_argument,
    duplicate_id,
    not_found,
    not_closable,
    limit_exceeded,
    callback_failed,
    active_child_changed,
};

struct MdiModelResult {
    MdiModelStatus status = MdiModelStatus::invalid_argument;
    std::optional<MdiChildId> child;
    std::exception_ptr error;
    explicit operator bool() const noexcept { return status == MdiModelStatus::success; }
};

// Pointer-free legacy-MDI ordering and activation state. Application document
// and view objects remain application-owned. Mutate this model on one UI thread
// when it is attached to MdiHost.
class MdiWorkspaceModel final {
public:
    static constexpr std::size_t MaximumChildren = 1024;

    MdiModelResult Add(MdiChildEntry child) noexcept;
    MdiModelResult Remove(MdiChildId child, bool force = false) noexcept;
    MdiModelResult Activate(MdiChildId child) noexcept;
    MdiModelResult Move(MdiChildId child, std::size_t index) noexcept;
    MdiModelResult TransferTo(MdiChildId child, MdiWorkspaceModel& destination,
                              std::size_t index) noexcept;
    MdiModelResult Rename(MdiChildId child, std::wstring title) noexcept;
    MdiModelResult SetDirty(MdiChildId child, bool dirty) noexcept;
    MdiModelResult SetClosable(MdiChildId child, bool closable) noexcept;

    MdiChildEntry* Find(MdiChildId child) noexcept;
    const MdiChildEntry* Find(MdiChildId child) const noexcept;
    std::span<const MdiChildEntry> GetChildren() const noexcept { return children_; }
    std::optional<MdiChildId> GetActiveId() const noexcept { return active_; }
    std::uint64_t GetRevision() const noexcept { return revision_; }

private:
    std::vector<MdiChildEntry> children_;
    std::optional<MdiChildId> active_;
    std::uint64_t revision_ = 0;
};

using MdiActiveChildHandler = std::function<void(MdiChildId)>;

// Snapshots the stable active ID, contains callback exceptions, and reports
// reentrant activation instead of silently continuing with a different child.
MdiModelResult RouteMdiActiveChild(const MdiWorkspaceModel& workspace,
                                   const MdiActiveChildHandler& handler) noexcept;

enum class MdiHostStatus {
    success,
    invalid_argument,
    invalid_state,
    wrong_thread,
    native_failure,
    model_failure,
    callback_failed,
};

struct MdiHostResult {
    MdiHostStatus status = MdiHostStatus::invalid_state;
    MdiChildId child{};
    HWND window = nullptr;
    DWORD error = ERROR_SUCCESS;
    std::exception_ptr exception;
    explicit operator bool() const noexcept { return status == MdiHostStatus::success; }
};

enum class MdiArrange { cascade, tile_horizontal, tile_vertical, arrange_icons };

using MdiChildMessageHandler =
    std::function<std::optional<LRESULT>(HWND, UINT, WPARAM, LPARAM)>;

struct MdiChildOptions {
    DWORD style = WS_VISIBLE | WS_OVERLAPPEDWINDOW;
    MdiChildMessageHandler message;
};

// Optional native legacy-MDI infrastructure. The host owns the MDICLIENT and
// child HWNDs but borrows the frame, menu, model, and application callbacks.
// All methods and destruction run on the creating UI thread. Callbacks are
// exception-contained; GetClient/GetChild return borrowed HWNDs.
class MdiHost final {
public:
    MdiHost() noexcept;
    ~MdiHost() noexcept;
    MdiHost(const MdiHost&) = delete;
    MdiHost& operator=(const MdiHost&) = delete;

    MdiHostResult Create(HWND frame, MdiWorkspaceModel& model,
                         HMENU window_menu = nullptr,
                         UINT first_child_command = 0xE000) noexcept;
    MdiHostResult CreateChild(MdiChildId child,
                              MdiChildOptions options = {}) noexcept;
    MdiHostResult Activate(MdiChildId child) noexcept;
    MdiHostResult Close(MdiChildId child, bool force = false) noexcept;
    MdiHostResult Arrange(MdiArrange arrangement) noexcept;
    bool Translate(MSG& message) const noexcept;
    LRESULT FrameDefault(UINT message, WPARAM wparam, LPARAM lparam) const noexcept;
    void Resize(RECT bounds) noexcept;
    void Destroy() noexcept;

    HWND GetClient() const noexcept { return client_; }
    HWND GetChild(MdiChildId child) const noexcept;
    std::optional<MdiChildId> GetChildId(HWND child) const noexcept;
    std::exception_ptr TakeCallbackException() noexcept;
    static LRESULT CALLBACK ChildProcedure(HWND, UINT, WPARAM, LPARAM) noexcept;

private:
    struct ChildState;
    void ChildDestroyed(ChildState* child) noexcept;
    MdiHostResult CheckThread() const noexcept;

    HWND frame_ = nullptr;
    HWND client_ = nullptr;
    DWORD thread_id_ = 0;
    MdiWorkspaceModel* model_ = nullptr;
    std::vector<std::unique_ptr<ChildState>> children_;
    std::exception_ptr callback_exception_;
};

}  // namespace mwtl
