#pragma once

#include <windows.h>

#include <cstdint>

#include <mwfl/concepts.h>
#include <mwfl/controls.h>
#include <mwfl/dpi.h>
#include <mwfl/events.h>

namespace mwfl {

enum class NativeHostState {
    uninitialized,
    ready,
    attached,
    destroyed,
};

// HWND-free lifecycle shared by NativeHost and pure application tests. Invalid
// transitions preserve state and return false. The nonzero attachment identity
// is opaque and is never dereferenced by the model.
class NativeHostStateModel final {
public:
    constexpr NativeHostState GetState() const noexcept { return state_; }
    constexpr std::uintptr_t GetAttachment() const noexcept { return attachment_; }
    constexpr bool IsAttached() const noexcept {
        return state_ == NativeHostState::attached && attachment_ != 0;
    }

    constexpr bool MarkCreated() noexcept {
        if (state_ != NativeHostState::uninitialized && state_ != NativeHostState::destroyed)
            return false;
        state_ = NativeHostState::ready;
        attachment_ = 0;
        return true;
    }
    constexpr bool Attach(std::uintptr_t identity) noexcept {
        if (state_ != NativeHostState::ready || identity == 0) return false;
        state_ = NativeHostState::attached;
        attachment_ = identity;
        return true;
    }
    constexpr std::uintptr_t Detach() noexcept {
        if (!IsAttached()) return 0;
        const std::uintptr_t identity = attachment_;
        attachment_ = 0;
        state_ = NativeHostState::ready;
        return identity;
    }
    constexpr void MarkDestroyed() noexcept {
        attachment_ = 0;
        state_ = NativeHostState::destroyed;
    }

private:
    NativeHostState state_ = NativeHostState::uninitialized;
    std::uintptr_t attachment_ = 0;
};

struct NativeHostOptions {
    DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
    DWORD extended_style = WS_EX_CONTROLPARENT;
};

// Owns one native container HWND and borrows one distinct direct-child HWND.
// Create the third-party child with GetHwnd() as parent, then Attach it. While
// attached, the child is resized to the client area, receives focus when the
// host receives focus, and its WM_NOTIFY, control-originated WM_COMMAND, and
// WM_CONTEXTMENU messages (including descendant sources) are synchronously
// forwarded to the host parent with exact parameters/results. Detach stops host
// management but does not destroy or reparent the child. If still attached,
// normal Win32 parent destruction destroys the child with the container.
// All operations belong to the creating UI thread; GetHwnd/GetChild are borrowed
// native escape hatches. Failure returns false/null and sets Win32 last-error.
class NativeHost final : public NativeControl {
public:
    NativeHost() noexcept = default;
    ~NativeHost() noexcept;

    NativeHost(const NativeHost&) = delete;
    NativeHost& operator=(const NativeHost&) = delete;
    NativeHost(NativeHost&&) = delete;
    NativeHost& operator=(NativeHost&&) = delete;

    bool Create(HWND parent, ControlId id, RectDip bounds, NativeHostOptions options = {});
    template <WindowLike Parent>
    bool Create(const Parent& parent, ControlId id, RectDip bounds,
                NativeHostOptions options = {}) {
        return Create(parent.GetHwnd(), id, bounds, options);
    }

    bool Attach(HWND child) noexcept;
    HWND Detach() noexcept;
    HWND GetChild() const noexcept;
    NativeHostState GetState() const noexcept { return model_.GetState(); }

    bool SetBounds(RectDip bounds) noexcept;
    bool Arrange() noexcept;

private:
    static LRESULT CALLBACK SubclassProcedure(HWND window, UINT message, WPARAM wparam,
                                              LPARAM lparam, UINT_PTR subclass_id,
                                              DWORD_PTR reference) noexcept;
    LRESULT ProcessMessage(HWND window, UINT message, WPARAM wparam, LPARAM lparam) noexcept;
    bool IsAttachedSource(HWND source) const noexcept;
    void ObserveDetachedChild(HWND child) noexcept;

    NativeHostStateModel model_;
    HWND child_ = nullptr;  // Borrowed direct child while attached.
};

}  // namespace mwfl
