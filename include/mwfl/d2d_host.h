#pragma once

#include <windows.h>
#include <d2d1.h>
#include <wrl/client.h>

#include <exception>
#include <functional>
#include <utility>

#include <mwfl/controls.h>

namespace mwfl {

enum class D2DRenderState { empty, ready, drawing, device_lost, destroyed };

// HWND/GPU-free transaction model used by D2DHost and deterministic tests.
class D2DRenderStateModel final {
public:
    constexpr D2DRenderState GetState() const noexcept { return state_; }
    constexpr bool MarkReady() noexcept {
        if (state_ == D2DRenderState::drawing || state_ == D2DRenderState::destroyed) return false;
        state_ = D2DRenderState::ready;
        return true;
    }
    constexpr bool BeginDraw() noexcept {
        if (state_ != D2DRenderState::ready) return false;
        state_ = D2DRenderState::drawing;
        return true;
    }
    constexpr bool EndDraw(HRESULT result) noexcept {
        if (state_ != D2DRenderState::drawing) return false;
        state_ = result == D2DERR_RECREATE_TARGET ? D2DRenderState::device_lost
                                                  : D2DRenderState::ready;
        return true;
    }
    constexpr void MarkDeviceLost() noexcept {
        if (state_ != D2DRenderState::destroyed) state_ = D2DRenderState::device_lost;
    }
    constexpr void MarkDestroyed() noexcept { state_ = D2DRenderState::destroyed; }

private:
    D2DRenderState state_ = D2DRenderState::empty;
};

struct D2DRenderContext {
    ID2D1HwndRenderTarget& target;
    D2D1_SIZE_F size_dip{};
    UINT dpi = 96;
    bool high_contrast = false;
};

struct D2DInputEvent {
    UINT message = 0;
    WPARAM wparam = 0;
    LPARAM lparam = 0;
    PointDip position{};
};

struct D2DHostCallbacks {
    // Called inside the one BeginDraw/EndDraw transaction owned by D2DHost.
    // Do not call BeginDraw or EndDraw and do not retain the context.
    std::function<void(D2DRenderContext&)> paint;
    // Called after a new render target exists and before its first paint.
    std::function<void(ID2D1HwndRenderTarget&)> create_device_resources;
    // Called before the target is released. Exceptions are contained.
    std::function<void()> discard_device_resources;
    // Mouse and keyboard input. Mouse positions are client DIPs. Return
    // Handled to consume the native message; exceptions are contained.
    std::function<EventResult(const D2DInputEvent&)> input;
};

struct D2DHostOptions {
    DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPSIBLINGS;
    DWORD extended_style = 0;
    D2D1_COLOR_F clear_color = D2D1::ColorF(D2D1::ColorF::White);
    D2DHostCallbacks callbacks;
};

// Optional Direct2D HWND render host. Link mwfl::d2d; the core mwfl::mwfl
// target does not link Direct2D. The host and callbacks belong to the creating
// UI thread. Geometry exposed to paint is in DIPs. The render target and callback
// context are borrowed escape hatches valid only until resources are discarded.
// Render owns every BeginDraw/EndDraw transaction, contains callback exceptions,
// and turns D2DERR_RECREATE_TARGET into a recoverable device_lost state.
class D2DHost final : public NativeControl {
public:
    D2DHost() noexcept = default;
    ~D2DHost() noexcept;
    D2DHost(const D2DHost&) = delete;
    D2DHost& operator=(const D2DHost&) = delete;
    D2DHost(D2DHost&&) = delete;
    D2DHost& operator=(D2DHost&&) = delete;

    bool Create(HWND parent, ControlId id, RectDip bounds, D2DHostOptions options = {});
    template <WindowLike Parent>
    bool Create(const Parent& parent, ControlId id, RectDip bounds,
                D2DHostOptions options = {}) {
        return Create(parent.GetHwnd(), id, bounds, std::move(options));
    }

    bool Render() noexcept;
    bool Invalidate() noexcept;
    void DiscardDeviceResources() noexcept;
    ID2D1HwndRenderTarget* GetRenderTarget() const noexcept { return render_target_.Get(); }
    D2DRenderState GetRenderState() const noexcept { return model_.GetState(); }
    HRESULT GetLastRenderError() const noexcept { return last_error_; }
    std::exception_ptr TakeCallbackException() noexcept;

private:
    static LRESULT CALLBACK SubclassProcedure(HWND window, UINT message, WPARAM wparam,
                                              LPARAM lparam, UINT_PTR subclass_id,
                                              DWORD_PTR reference) noexcept;
    LRESULT ProcessMessage(HWND window, UINT message, WPARAM wparam, LPARAM lparam) noexcept;
    bool EnsureDeviceResources() noexcept;
    bool ResizeTarget() noexcept;
    bool IsHighContrast() const noexcept;
    void RecordCallbackException() noexcept;

    D2DHostOptions options_;
    D2DRenderStateModel model_;
    Microsoft::WRL::ComPtr<ID2D1Factory> factory_;
    Microsoft::WRL::ComPtr<ID2D1HwndRenderTarget> render_target_;
    HRESULT last_error_ = S_OK;
    std::exception_ptr callback_exception_;
};

}  // namespace mwfl
