#include <mwfl/d2d_host.h>
#include <mwfl/appearance.h>

#include <commctrl.h>
#include <windowsx.h>

#include <algorithm>
#include <utility>

namespace mwfl {
namespace {
constexpr UINT_PTR kD2DHostSubclassId = 1;
}

D2DHost::~D2DHost() noexcept {
    Destroy();
    DiscardDeviceResources();
    factory_.Reset();
}

bool D2DHost::Create(HWND parent, ControlId id, RectDip bounds, D2DHostOptions options) {
    DiscardDeviceResources();
    factory_.Reset();
    model_ = {};
    callback_exception_ = {};
    last_error_ = S_OK;
    options_ = std::move(options);
    if (!CreateNative(L"STATIC", parent, id, L"Direct2D drawing surface", bounds,
                      options_.style, options_.extended_style))
        return false;
    if (::SetWindowSubclass(GetHwnd(), SubclassProcedure, kD2DHostSubclassId,
                            reinterpret_cast<DWORD_PTR>(this)) == FALSE) {
        Destroy();
        model_.MarkDestroyed();
        return false;
    }
    const D2D1_FACTORY_OPTIONS factory_options{};
    last_error_ = ::D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
                                      factory_options, factory_.ReleaseAndGetAddressOf());
    if (FAILED(last_error_)) {
        Destroy();
        model_.MarkDestroyed();
        ::SetLastError(static_cast<DWORD>(last_error_));
        return false;
    }
    if (!SetAccessibleName(GetHwnd(), L"Direct2D drawing surface") ||
        !EnsureDeviceResources()) {
        const HRESULT error = FAILED(last_error_) ? last_error_ : E_FAIL;
        Destroy();
        DiscardDeviceResources();
        factory_.Reset();
        model_.MarkDestroyed();
        last_error_ = error;
        return false;
    }
    return true;
}

bool D2DHost::EnsureDeviceResources() noexcept {
    if (render_target_) return true;
    const HWND window = GetHwnd();
    if (window == nullptr || !factory_) {
        last_error_ = E_HANDLE;
        return false;
    }
    RECT client{};
    if (::GetClientRect(window, &client) == FALSE) {
        last_error_ = HRESULT_FROM_WIN32(::GetLastError());
        return false;
    }
    const UINT dpi = DpiContext::FromWindow(window).GetDpi();
    const auto size = D2D1::SizeU(static_cast<UINT>((std::max)(1L, client.right - client.left)),
                                  static_cast<UINT>((std::max)(1L, client.bottom - client.top)));
    last_error_ = factory_->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,
                                     D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN,
                                                       D2D1_ALPHA_MODE_UNKNOWN),
                                     static_cast<float>(dpi), static_cast<float>(dpi)),
        D2D1::HwndRenderTargetProperties(window, size), render_target_.ReleaseAndGetAddressOf());
    if (FAILED(last_error_)) return false;
    if (options_.callbacks.create_device_resources) {
        try {
            options_.callbacks.create_device_resources(*render_target_.Get());
        } catch (...) {
            RecordCallbackException();
            DiscardDeviceResources();
            last_error_ = E_UNEXPECTED;
            return false;
        }
    }
    model_.MarkReady();
    return true;
}

bool D2DHost::Render() noexcept {
    RECT client{};
    if (GetHwnd() == nullptr || ::GetClientRect(GetHwnd(), &client) == FALSE) {
        last_error_ = E_HANDLE;
        return false;
    }
    // Minimized/zero-area hosts have no frame to draw and keep resources for
    // deterministic restoration; this is a successful no-op.
    if (client.right <= client.left || client.bottom <= client.top) return true;
    if (!EnsureDeviceResources() || !model_.BeginDraw()) return false;
    render_target_->BeginDraw();
    render_target_->Clear(options_.clear_color);
    if (options_.callbacks.paint) {
        try {
            const D2D1_SIZE_F size = render_target_->GetSize();
            D2DRenderContext context{*render_target_.Get(), size,
                                     DpiContext::FromWindow(GetHwnd()).GetDpi(),
                                     IsHighContrast()};
            options_.callbacks.paint(context);
        } catch (...) {
            RecordCallbackException();
        }
    }
    last_error_ = render_target_->EndDraw();
    model_.EndDraw(last_error_);
    if (last_error_ == D2DERR_RECREATE_TARGET) {
        DiscardDeviceResources();
        model_.MarkDeviceLost();
        Invalidate();
        return false;
    }
    if (callback_exception_) {
        last_error_ = E_UNEXPECTED;
        return false;
    }
    return SUCCEEDED(last_error_);
}

bool D2DHost::ResizeTarget() noexcept {
    if (!render_target_) return true;
    RECT client{};
    if (::GetClientRect(GetHwnd(), &client) == FALSE) return false;
    const LONG width = client.right - client.left;
    const LONG height = client.bottom - client.top;
    if (width <= 0 || height <= 0) return true;
    last_error_ = render_target_->Resize(
        D2D1::SizeU(static_cast<UINT>(width), static_cast<UINT>(height)));
    if (last_error_ == D2DERR_RECREATE_TARGET) {
        DiscardDeviceResources();
        model_.MarkDeviceLost();
        return false;
    }
    return SUCCEEDED(last_error_);
}

void D2DHost::DiscardDeviceResources() noexcept {
    if (!render_target_) return;
    if (options_.callbacks.discard_device_resources) {
        try {
            options_.callbacks.discard_device_resources();
        } catch (...) {
            RecordCallbackException();
        }
    }
    render_target_.Reset();
}

bool D2DHost::Invalidate() noexcept {
    const HWND window = GetHwnd();
    return window != nullptr && ::InvalidateRect(window, nullptr, FALSE) != FALSE;
}

std::exception_ptr D2DHost::TakeCallbackException() noexcept {
    return std::exchange(callback_exception_, {});
}

void D2DHost::RecordCallbackException() noexcept {
    callback_exception_ = std::current_exception();
}

bool D2DHost::IsHighContrast() const noexcept {
    HIGHCONTRASTW high_contrast{};
    high_contrast.cbSize = sizeof(high_contrast);
    return ::SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(high_contrast), &high_contrast, 0) !=
               FALSE &&
           (high_contrast.dwFlags & HCF_HIGHCONTRASTON) != 0;
}

LRESULT CALLBACK D2DHost::SubclassProcedure(HWND window, UINT message, WPARAM wparam,
                                            LPARAM lparam, UINT_PTR subclass_id,
                                            DWORD_PTR reference) noexcept {
    auto* self = reinterpret_cast<D2DHost*>(reference);
    if (self == nullptr) return ::DefSubclassProc(window, message, wparam, lparam);
    if (message == WM_NCDESTROY) {
        self->DiscardDeviceResources();
        self->factory_.Reset();
        self->model_.MarkDestroyed();
        ::RemoveWindowSubclass(window, SubclassProcedure, subclass_id);
        return ::DefSubclassProc(window, message, wparam, lparam);
    }
    return self->ProcessMessage(window, message, wparam, lparam);
}

LRESULT D2DHost::ProcessMessage(HWND window, UINT message, WPARAM wparam,
                                LPARAM lparam) noexcept {
    const bool is_mouse = message >= WM_MOUSEFIRST && message <= WM_MOUSELAST;
    const bool is_keyboard = message >= WM_KEYFIRST && message <= WM_KEYLAST;
    if ((is_mouse || is_keyboard) && options_.callbacks.input) {
        if (message == WM_LBUTTONDOWN || message == WM_MBUTTONDOWN ||
            message == WM_RBUTTONDOWN)
            ::SetFocus(window);
        const DpiContext dpi = DpiContext::FromWindow(window);
        POINT mouse_position{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        if (message == WM_MOUSEWHEEL || message == WM_MOUSEHWHEEL)
            ::ScreenToClient(window, &mouse_position);
        const D2DInputEvent event{
            message, wparam, lparam,
            is_mouse ? PointDip{dpi.FromPixels(mouse_position.x),
                                dpi.FromPixels(mouse_position.y)}
                     : PointDip{}};
        try {
            const EventResult result = options_.callbacks.input(event);
            if (result.handled) return result.result;
        } catch (...) {
            RecordCallbackException();
            last_error_ = E_UNEXPECTED;
            return 0;
        }
    }
    switch (message) {
        case WM_PAINT: {
            PAINTSTRUCT paint{};
            ::BeginPaint(window, &paint);
            ::EndPaint(window, &paint);
            Render();
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_SIZE:
            ResizeTarget();
            Invalidate();
            return 0;
        case WM_DPICHANGED_AFTERPARENT:
            if (render_target_) {
                const float dpi = static_cast<float>(DpiContext::FromWindow(window).GetDpi());
                render_target_->SetDpi(dpi, dpi);
            }
            ResizeTarget();
            Invalidate();
            return 0;
        case WM_THEMECHANGED:
        case WM_SYSCOLORCHANGE:
        case WM_SETTINGCHANGE:
            DiscardDeviceResources();
            Invalidate();
            return 0;
        default:
            return ::DefSubclassProc(window, message, wparam, lparam);
    }
}

}  // namespace mwfl
