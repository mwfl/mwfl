#pragma once

#include <windows.h>
#include <shellapi.h>

#include <optional>
#include <string>
#include <string_view>

#include <mwtl/events.h>

namespace mwtl {

enum class TrayIconState {
    detached,
    added,
    recovery_pending,
};

enum class TrayIconEventKind {
    primary_activate,
    context_menu,
    balloon_shown,
    balloon_hidden,
    balloon_timed_out,
    balloon_clicked,
    popup_opened,
    popup_closed,
    unknown,
};

struct TrayIconEvent {
    TrayIconEventKind kind = TrayIconEventKind::unknown;
    UINT native_code = 0;
    POINT screen_position{};
    bool keyboard = false;
};

struct TrayIconOptions {
    HWND owner = nullptr;  // Borrowed and must outlive the icon.
    UINT id = 1;           // Stable nonzero callback identifier, limited to 16 bits.
    UINT callback_message = WM_APP + 1;
    GUID identity{};       // Stable application-defined GUID used by the shell.
    HICON icon = nullptr;  // Borrowed until UpdateIcon, Remove, or destruction.
    std::wstring tooltip;
    bool hidden = false;
};

struct TrayNotification {
    std::wstring title;
    std::wstring text;
    DWORD flags = NIIF_INFO;
    bool respect_quiet_time = true;
};

// Move-only owner for one Windows notification-area registration. The owner
// HWND, HICON, and callback message are borrowed. Mutating operations belong to
// the creating UI thread. Removal is attempted exactly once by destruction.
class TrayIcon final {
   public:
    TrayIcon() noexcept = default;
    ~TrayIcon() noexcept;

    TrayIcon(const TrayIcon&) = delete;
    TrayIcon& operator=(const TrayIcon&) = delete;
    TrayIcon(TrayIcon&& other) noexcept;
    TrayIcon& operator=(TrayIcon&& other) noexcept;

    bool Add(TrayIconOptions options) noexcept;
    bool UpdateTooltip(std::wstring_view tooltip) noexcept;
    bool UpdateIcon(HICON icon) noexcept;
    bool SetVisible(bool visible) noexcept;
    bool ShowNotification(const TrayNotification& notification) noexcept;

    // Call after receiving the registered TaskbarCreated message. Recreate
    // deletes any stale registration before adding the current state again and
    // keeps recovery_pending on failure so a later retry is explicit.
    bool Recreate() noexcept;
    bool IsTaskbarCreated(const WindowMessage& message) const noexcept;
    std::optional<TrayIconEvent> Decode(const WindowMessage& message) const noexcept;

    void Remove() noexcept;
    TrayIconState GetState() const noexcept { return state_; }
    bool IsAdded() const noexcept { return state_ == TrayIconState::added; }
    bool IsOwnerThread() const noexcept;
    const TrayIconOptions& GetOptions() const noexcept { return options_; }

    static UINT GetTaskbarCreatedMessage() noexcept;

   private:
    bool AddNative() noexcept;
    bool Modify(NOTIFYICONDATAW& data) noexcept;
    bool ValidateThread() const noexcept;
    NOTIFYICONDATAW BaseData() const noexcept;

    TrayIconOptions options_;
    TrayIconState state_ = TrayIconState::detached;
    DWORD owner_thread_ = 0;
};

}  // namespace mwtl
