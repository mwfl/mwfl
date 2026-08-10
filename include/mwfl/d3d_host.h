#pragma once

#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#include <exception>
#include <functional>
#include <utility>

#include <mwfl/controls.h>

namespace mwfl {

enum class D3DRenderState { empty, ready, rendering, minimized, occluded, device_lost, destroyed };

constexpr bool IsD3DDeviceLoss(HRESULT result) noexcept {
    return result == DXGI_ERROR_DEVICE_REMOVED || result == DXGI_ERROR_DEVICE_RESET ||
           result == DXGI_ERROR_DRIVER_INTERNAL_ERROR;
}

class D3DRenderStateModel final {
public:
    constexpr D3DRenderState GetState() const noexcept { return state_; }
    constexpr bool UsesSoftwareAdapter() const noexcept { return software_; }
    constexpr bool MarkReady(bool software = false) noexcept {
        if (state_ == D3DRenderState::rendering || state_ == D3DRenderState::destroyed) return false;
        state_ = D3DRenderState::ready;
        software_ = software;
        return true;
    }
    constexpr bool BeginFrame() noexcept {
        if (state_ != D3DRenderState::ready) return false;
        state_ = D3DRenderState::rendering;
        return true;
    }
    constexpr bool EndFrame(HRESULT result) noexcept {
        if (state_ != D3DRenderState::rendering) return false;
        state_ = result == DXGI_STATUS_OCCLUDED
            ? D3DRenderState::occluded
            : (IsD3DDeviceLoss(result) ? D3DRenderState::device_lost : D3DRenderState::ready);
        return true;
    }
    constexpr void MarkMinimized() noexcept {
        if (state_ != D3DRenderState::destroyed) state_ = D3DRenderState::minimized;
    }
    constexpr void MarkDeviceLost() noexcept {
        if (state_ != D3DRenderState::destroyed) state_ = D3DRenderState::device_lost;
    }
    constexpr void MarkDestroyed() noexcept { state_ = D3DRenderState::destroyed; }

private:
    D3DRenderState state_ = D3DRenderState::empty;
    bool software_ = false;
};

enum class D3DFrameStatus { presented, minimized, occluded, device_recreated, failed };

struct D3DFrameResult {
    D3DFrameStatus status = D3DFrameStatus::failed;
    HRESULT error = E_FAIL;
    explicit operator bool() const noexcept {
        return status == D3DFrameStatus::presented || status == D3DFrameStatus::minimized ||
               status == D3DFrameStatus::device_recreated;
    }
};

struct D3DFrameContext {
    ID3D11Device& device;
    ID3D11DeviceContext& context;
    ID3D11RenderTargetView& render_target;
    SIZE size_pixels{};
    SizeDip size_dip{};
    UINT dpi = 96;
};

struct D3DHostCallbacks {
    std::function<void(ID3D11Device&, ID3D11DeviceContext&)> create_device_resources;
    std::function<void()> discard_device_resources;
    std::function<void(D3DFrameContext&)> render;
};

struct D3DHostOptions {
    DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPSIBLINGS;
    DWORD extended_style = 0;
    bool allow_hardware = true;
    bool allow_warp_fallback = true;
    bool vertical_sync = true;
    D3DHostCallbacks callbacks;
};

// Optional on-demand Direct3D 11 flip-model swap-chain host. Link mwfl::d3d.
// It owns HWND/device/context/swap chain/RTV and borrows callback-scoped escape
// hatches. It renders only when RenderFrame or WM_PAINT requests a frame and
// therefore imposes no game loop. BGRA8 UNORM with ignored alpha is presented.
// All methods and callbacks belong to the creating UI thread. Device removal is
// reported and synchronously recreated when possible; WARP fallback is explicit
// through UsesSoftwareAdapter. Application data must remain device-independent.
class D3DHost final : public NativeControl {
public:
    D3DHost() noexcept = default;
    ~D3DHost() noexcept;
    D3DHost(const D3DHost&) = delete;
    D3DHost& operator=(const D3DHost&) = delete;
    D3DHost(D3DHost&&) = delete;
    D3DHost& operator=(D3DHost&&) = delete;

    bool Create(HWND parent, ControlId id, RectDip bounds, D3DHostOptions options = {});
    template <WindowLike Parent>
    bool Create(const Parent& parent, ControlId id, RectDip bounds,
                D3DHostOptions options = {}) {
        return Create(parent.GetHwnd(), id, bounds, std::move(options));
    }

    D3DFrameResult RenderFrame() noexcept;
    bool ResizeSwapChain() noexcept;
    bool Invalidate() noexcept;
    void DiscardDevice() noexcept;

    D3DRenderState GetRenderState() const noexcept { return model_.GetState(); }
    bool UsesSoftwareAdapter() const noexcept { return model_.UsesSoftwareAdapter(); }
    HRESULT GetLastError() const noexcept { return last_error_; }
    std::exception_ptr TakeCallbackException() noexcept;
    ID3D11Device* GetDevice() const noexcept { return device_.Get(); }
    IDXGISwapChain1* GetSwapChain() const noexcept { return swap_chain_.Get(); }

private:
    static LRESULT CALLBACK SubclassProcedure(HWND window, UINT message, WPARAM wparam,
                                              LPARAM lparam, UINT_PTR subclass_id,
                                              DWORD_PTR reference) noexcept;
    LRESULT ProcessMessage(HWND window, UINT message, WPARAM wparam, LPARAM lparam) noexcept;
    bool CreateDeviceAndSwapChain() noexcept;
    bool CreateDevice(D3D_DRIVER_TYPE driver) noexcept;
    bool CreateRenderTarget() noexcept;
    bool RecoverDevice() noexcept;
    void DiscardRenderTarget() noexcept;
    void RecordCallbackException() noexcept;
    SIZE GetClientSize() const noexcept;

    D3DHostOptions options_;
    D3DRenderStateModel model_;
    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> render_target_;
    D3D_FEATURE_LEVEL feature_level_ = D3D_FEATURE_LEVEL_10_0;
    HRESULT last_error_ = S_OK;
    std::exception_ptr callback_exception_;
    bool resize_pending_ = false;
};

}  // namespace mwfl
