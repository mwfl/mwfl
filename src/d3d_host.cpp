#include <mwfl/d3d_host.h>
#include <mwfl/appearance.h>

#include <commctrl.h>

#include <algorithm>
#include <iterator>

namespace mwfl {
namespace {
constexpr UINT_PTR kD3DHostSubclassId = 1;
}

D3DHost::~D3DHost() noexcept {
    Destroy();
    DiscardDevice();
}

bool D3DHost::Create(HWND parent, ControlId id, RectDip bounds, D3DHostOptions options) {
    DiscardDevice();
    model_ = {};
    options_ = std::move(options);
    last_error_ = S_OK;
    callback_exception_ = {};
    resize_pending_ = false;
    if (!options_.allow_hardware && !options_.allow_warp_fallback) {
        last_error_ = E_INVALIDARG;
        return false;
    }
    if (!CreateNative(L"STATIC", parent, id, L"Direct3D rendering surface", bounds,
                      options_.style, options_.extended_style))
        return false;
    if (!::SetWindowSubclass(GetHwnd(), SubclassProcedure, kD3DHostSubclassId,
                             reinterpret_cast<DWORD_PTR>(this)) ||
        !SetAccessibleName(GetHwnd(), L"Direct3D rendering surface") ||
        !CreateDeviceAndSwapChain()) {
        const HRESULT error = FAILED(last_error_) ? last_error_ : E_FAIL;
        Destroy();
        DiscardDevice();
        model_.MarkDestroyed();
        last_error_ = error;
        return false;
    }
    return true;
}

bool D3DHost::CreateDevice(D3D_DRIVER_TYPE driver) noexcept {
    constexpr D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0,
                                            D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0};
    device_.Reset();
    context_.Reset();
    last_error_ = ::D3D11CreateDevice(nullptr, driver, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                                      levels, static_cast<UINT>(std::size(levels)),
                                      D3D11_SDK_VERSION, device_.ReleaseAndGetAddressOf(),
                                      &feature_level_, context_.ReleaseAndGetAddressOf());
    return SUCCEEDED(last_error_);
}

bool D3DHost::CreateDeviceAndSwapChain() noexcept {
    bool software = false;
    if (!options_.allow_hardware || !CreateDevice(D3D_DRIVER_TYPE_HARDWARE)) {
        if (!options_.allow_warp_fallback || !CreateDevice(D3D_DRIVER_TYPE_WARP)) return false;
        software = true;
    }

    Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device;
    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
    Microsoft::WRL::ComPtr<IDXGIFactory2> factory;
    last_error_ = device_.As(&dxgi_device);
    if (SUCCEEDED(last_error_)) last_error_ = dxgi_device->GetAdapter(&adapter);
    if (SUCCEEDED(last_error_)) last_error_ = adapter->GetParent(IID_PPV_ARGS(&factory));
    if (FAILED(last_error_)) return false;

    const SIZE size = GetClientSize();
    DXGI_SWAP_CHAIN_DESC1 description{};
    description.Width = static_cast<UINT>((std::max)(size.cx, 1L));
    description.Height = static_cast<UINT>((std::max)(size.cy, 1L));
    description.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    description.SampleDesc = {1, 0};
    description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    description.BufferCount = 2;
    description.Scaling = DXGI_SCALING_STRETCH;
    description.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    description.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
    last_error_ = factory->CreateSwapChainForHwnd(device_.Get(), GetHwnd(), &description,
                                                  nullptr, nullptr,
                                                  swap_chain_.ReleaseAndGetAddressOf());
    if (FAILED(last_error_)) return false;
    factory->MakeWindowAssociation(GetHwnd(), DXGI_MWA_NO_ALT_ENTER);
    if (!CreateRenderTarget()) return false;
    if (options_.callbacks.create_device_resources) {
        try {
            options_.callbacks.create_device_resources(*device_.Get(), *context_.Get());
        } catch (...) {
            RecordCallbackException();
            last_error_ = E_UNEXPECTED;
            return false;
        }
    }
    model_.MarkReady(software);
    return true;
}

bool D3DHost::CreateRenderTarget() noexcept {
    Microsoft::WRL::ComPtr<ID3D11Texture2D> buffer;
    last_error_ = swap_chain_->GetBuffer(0, IID_PPV_ARGS(&buffer));
    if (FAILED(last_error_)) return false;
    last_error_ = device_->CreateRenderTargetView(buffer.Get(), nullptr,
                                                  render_target_.ReleaseAndGetAddressOf());
    return SUCCEEDED(last_error_);
}

D3DFrameResult D3DHost::RenderFrame() noexcept {
    const SIZE size = GetClientSize();
    if (size.cx <= 0 || size.cy <= 0) {
        model_.MarkMinimized();
        return {D3DFrameStatus::minimized, S_OK};
    }
    if (model_.GetState() == D3DRenderState::occluded) {
        last_error_ = swap_chain_->Present(0, DXGI_PRESENT_TEST);
        if (last_error_ == DXGI_STATUS_OCCLUDED)
            return {D3DFrameStatus::occluded, last_error_};
        if (IsD3DDeviceLoss(last_error_)) return RecoverDevice()
            ? D3DFrameResult{D3DFrameStatus::device_recreated, S_OK}
            : D3DFrameResult{D3DFrameStatus::failed, last_error_};
        model_.MarkReady(model_.UsesSoftwareAdapter());
    }
    if (model_.GetState() == D3DRenderState::device_lost) {
        return RecoverDevice() ? D3DFrameResult{D3DFrameStatus::device_recreated, S_OK}
                               : D3DFrameResult{D3DFrameStatus::failed, last_error_};
    }
    if (!device_ || !context_ || !swap_chain_ || !render_target_ || !model_.BeginFrame())
        return {D3DFrameStatus::failed, E_UNEXPECTED};

    if (options_.callbacks.render) {
        try {
            const DpiContext dpi = DpiContext::FromWindow(GetHwnd());
            D3DFrameContext frame{*device_.Get(), *context_.Get(), *render_target_.Get(), size,
                                  {dpi.FromPixels(size.cx), dpi.FromPixels(size.cy)}, dpi.GetDpi()};
            options_.callbacks.render(frame);
        } catch (...) {
            RecordCallbackException();
        }
    }
    if (model_.GetState() == D3DRenderState::destroyed || GetHwnd() == nullptr ||
        !swap_chain_) {
        last_error_ = HRESULT_FROM_WIN32(ERROR_CANCELLED);
        return {D3DFrameStatus::failed, last_error_};
    }
    const HRESULT frame_result = callback_exception_
        ? E_UNEXPECTED
        : swap_chain_->Present(options_.vertical_sync ? 1 : 0, 0);
    model_.EndFrame(frame_result);
    last_error_ = frame_result;
    if (IsD3DDeviceLoss(frame_result)) {
        resize_pending_ = false;
        return RecoverDevice() ? D3DFrameResult{D3DFrameStatus::device_recreated, S_OK}
                               : D3DFrameResult{D3DFrameStatus::failed, last_error_};
    }
    if (resize_pending_) {
        resize_pending_ = false;
        if (!ResizeSwapChain()) return {D3DFrameStatus::failed, last_error_};
    }
    last_error_ = frame_result;
    if (frame_result == DXGI_STATUS_OCCLUDED)
        return {D3DFrameStatus::occluded, frame_result};
    return SUCCEEDED(frame_result) ? D3DFrameResult{D3DFrameStatus::presented, S_OK}
                                   : D3DFrameResult{D3DFrameStatus::failed, frame_result};
}

bool D3DHost::ResizeSwapChain() noexcept {
    if (model_.GetState() == D3DRenderState::rendering) {
        resize_pending_ = true;
        return true;
    }
    const SIZE size = GetClientSize();
    if (size.cx <= 0 || size.cy <= 0) {
        model_.MarkMinimized();
        return true;
    }
    if (!swap_chain_) return false;
    context_->OMSetRenderTargets(0, nullptr, nullptr);
    DiscardRenderTarget();
    last_error_ = swap_chain_->ResizeBuffers(0, static_cast<UINT>(size.cx),
                                             static_cast<UINT>(size.cy),
                                             DXGI_FORMAT_UNKNOWN, 0);
    if (IsD3DDeviceLoss(last_error_)) {
        model_.MarkDeviceLost();
        return RecoverDevice();
    }
    if (FAILED(last_error_) || !CreateRenderTarget()) return false;
    model_.MarkReady(model_.UsesSoftwareAdapter());
    return true;
}

bool D3DHost::RecoverDevice() noexcept {
    DiscardDevice();
    const bool recreated = CreateDeviceAndSwapChain();
    if (!recreated) model_.MarkDeviceLost();
    return recreated;
}

void D3DHost::DiscardRenderTarget() noexcept { render_target_.Reset(); }

void D3DHost::DiscardDevice() noexcept {
    if (options_.callbacks.discard_device_resources && device_) {
        try {
            options_.callbacks.discard_device_resources();
        } catch (...) {
            RecordCallbackException();
        }
    }
    DiscardRenderTarget();
    swap_chain_.Reset();
    context_.Reset();
    device_.Reset();
    resize_pending_ = false;
}

bool D3DHost::Invalidate() noexcept {
    return GetHwnd() != nullptr && ::InvalidateRect(GetHwnd(), nullptr, FALSE) != FALSE;
}

std::exception_ptr D3DHost::TakeCallbackException() noexcept {
    return std::exchange(callback_exception_, {});
}

void D3DHost::RecordCallbackException() noexcept { callback_exception_ = std::current_exception(); }

SIZE D3DHost::GetClientSize() const noexcept {
    RECT client{};
    if (GetHwnd() != nullptr) ::GetClientRect(GetHwnd(), &client);
    return {client.right - client.left, client.bottom - client.top};
}

LRESULT CALLBACK D3DHost::SubclassProcedure(HWND window, UINT message, WPARAM wparam,
                                            LPARAM lparam, UINT_PTR subclass_id,
                                            DWORD_PTR reference) noexcept {
    auto* self = reinterpret_cast<D3DHost*>(reference);
    if (!self) return ::DefSubclassProc(window, message, wparam, lparam);
    if (message == WM_NCDESTROY) {
        self->DiscardDevice();
        self->model_.MarkDestroyed();
        ::RemoveWindowSubclass(window, SubclassProcedure, subclass_id);
        return ::DefSubclassProc(window, message, wparam, lparam);
    }
    return self->ProcessMessage(window, message, wparam, lparam);
}

LRESULT D3DHost::ProcessMessage(HWND window, UINT message, WPARAM wparam,
                                LPARAM lparam) noexcept {
    switch (message) {
        case WM_PAINT: {
            PAINTSTRUCT paint{};
            ::BeginPaint(window, &paint);
            ::EndPaint(window, &paint);
            RenderFrame();
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_SIZE:
        case WM_DPICHANGED_AFTERPARENT:
            ResizeSwapChain();
            Invalidate();
            return 0;
        case WM_THEMECHANGED:
        case WM_SYSCOLORCHANGE:
        case WM_SETTINGCHANGE:
            Invalidate();
            return 0;
        default:
            return ::DefSubclassProc(window, message, wparam, lparam);
    }
}

}  // namespace mwfl
