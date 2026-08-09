#include <mwtl/d2d_host.h>

#include <stdexcept>

namespace {
HWND CreateParent() {
    return ::CreateWindowExW(0, L"STATIC", L"d2d-test", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                             0, 0, 420, 300, nullptr, nullptr, ::GetModuleHandleW(nullptr),
                             nullptr);
}
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    using namespace mwtl;
    const HWND parent = CreateParent();
    if (parent == nullptr) return 1;

    int creates = 0;
    int paints = 0;
    int discards = 0;
    int inputs = 0;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
    D2DHost host;
    D2DHostOptions options;
    options.callbacks.create_device_resources = [&](ID2D1HwndRenderTarget& target) {
        ++creates;
        const HRESULT result = target.CreateSolidColorBrush(
            D2D1::ColorF(D2D1::ColorF::CornflowerBlue), brush.ReleaseAndGetAddressOf());
        if (FAILED(result)) throw std::runtime_error("brush");
    };
    options.callbacks.paint = [&](D2DRenderContext& context) {
        ++paints;
        if (context.dpi < 96 || context.size_dip.width <= 0 || context.size_dip.height <= 0 ||
            !brush)
            throw std::runtime_error("context");
        context.target.FillRectangle(D2D1::RectF(8, 8, context.size_dip.width - 8,
                                                  context.size_dip.height - 8),
                                     brush.Get());
    };
    options.callbacks.discard_device_resources = [&] {
        ++discards;
        brush.Reset();
    };
    options.callbacks.input = [&](const D2DInputEvent& event) {
        if (event.message == WM_LBUTTONDOWN && event.position.x.value >= 0) ++inputs;
        return EventResult::Handled(77);
    };
    if (!host.Create(parent, ControlId{701}, {0.0_dip, 0.0_dip, 300.0_dip, 180.0_dip},
                     std::move(options)))
        return 2;
    if (host.GetRenderTarget() == nullptr || host.GetRenderState() != D2DRenderState::ready ||
        creates != 1)
        return 3;
    if (!host.Render() || paints != 1 || FAILED(host.GetLastRenderError())) return 4;
    if (!host.SetBounds({0.0_dip, 0.0_dip, 240.0_dip, 140.0_dip}) || !host.Render()) return 5;
    const D2D1_SIZE_U pixels = host.GetRenderTarget()->GetPixelSize();
    if (pixels.width == 0 || pixels.height == 0) return 6;
    ::SendMessageW(host.GetHwnd(), WM_THEMECHANGED, 0, 0);
    ::SendMessageW(host.GetHwnd(), WM_DPICHANGED_AFTERPARENT, 0, 0);
    if (::SendMessageW(host.GetHwnd(), WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(12, 14)) != 77 ||
        inputs != 1 || ::GetFocus() != host.GetHwnd())
        return 7;

    ::SetWindowPos(host.GetHwnd(), nullptr, 0, 0, 0, 0, SWP_NOZORDER | SWP_NOACTIVATE);
    if (!host.Render()) return 8;
    if (!host.SetBounds({0.0_dip, 0.0_dip, 240.0_dip, 140.0_dip})) return 9;

    host.DiscardDeviceResources();
    if (host.GetRenderTarget() != nullptr || discards != 1) return 10;
    if (!host.Render() || creates != 2 || paints != 3) return 11;

    host.Destroy();
    if (host.GetRenderState() != D2DRenderState::destroyed || discards != 2 || brush) return 12;

    D2DHost exceptional;
    D2DHostOptions exceptional_options;
    exceptional_options.callbacks.paint = [](D2DRenderContext&) {
        throw std::runtime_error("paint");
    };
    if (!exceptional.Create(parent, ControlId{702},
                            {0.0_dip, 0.0_dip, 100.0_dip, 80.0_dip},
                            std::move(exceptional_options)))
        return 13;
    if (exceptional.Render() || exceptional.GetLastRenderError() != E_UNEXPECTED ||
        !exceptional.TakeCallbackException() || exceptional.TakeCallbackException())
        return 14;
    exceptional.Destroy();
    ::DestroyWindow(parent);
    return 0;
}
