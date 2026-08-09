#include <mwtl/d3d_host.h>

#include <stdexcept>

namespace {
HWND CreateParent() {
    return ::CreateWindowExW(0, L"STATIC", L"d3d-test", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                             0, 0, 440, 320, nullptr, nullptr, ::GetModuleHandleW(nullptr),
                             nullptr);
}

DWORD GuiResources() {
    return ::GetGuiResources(::GetCurrentProcess(), GR_USEROBJECTS) +
           ::GetGuiResources(::GetCurrentProcess(), GR_GDIOBJECTS);
}
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    using namespace mwtl;
    const HWND parent = CreateParent();
    if (!parent) return 1;

    int creates = 0;
    int renders = 0;
    int discards = 0;
    D3DHost host;
    D3DHostOptions options;
    options.vertical_sync = false;
    options.callbacks.create_device_resources = [&](ID3D11Device&, ID3D11DeviceContext&) {
        ++creates;
    };
    options.callbacks.discard_device_resources = [&] { ++discards; };
    options.callbacks.render = [&](D3DFrameContext& frame) {
        ++renders;
        if (frame.size_pixels.cx <= 0 || frame.size_dip.width.value <= 0 || frame.dpi < 96)
            throw std::runtime_error("frame context");
        const float color[] = {0.08f, 0.25f, 0.55f, 1.0f};
        ID3D11RenderTargetView* target = &frame.render_target;
        frame.context.OMSetRenderTargets(1, &target, nullptr);
        frame.context.ClearRenderTargetView(target, color);
    };
    if (!host.Create(parent, {711}, {0.0_dip, 0.0_dip, 320.0_dip, 200.0_dip},
                     std::move(options)))
        return 2;
    if (!host.GetDevice() || !host.GetSwapChain() || creates != 1 ||
        host.GetRenderState() != D3DRenderState::ready)
        return 3;
    const auto frame = host.RenderFrame();
    if (!frame || frame.status != D3DFrameStatus::presented || renders != 1) return 4;

    ::SetWindowPos(host.GetHwnd(), nullptr, 0, 0, 0, 0, SWP_NOZORDER | SWP_NOACTIVATE);
    const auto minimized = host.RenderFrame();
    if (!minimized || minimized.status != D3DFrameStatus::minimized ||
        host.GetRenderState() != D3DRenderState::minimized)
        return 5;
    if (!host.SetBounds({0.0_dip, 0.0_dip, 240.0_dip, 150.0_dip}) ||
        !host.ResizeSwapChain() || !host.RenderFrame())
        return 6;

    D3DHost warp;
    D3DHostOptions warp_options;
    warp_options.allow_hardware = false;
    warp_options.allow_warp_fallback = true;
    warp_options.vertical_sync = false;
    if (!warp.Create(parent, {712}, {0.0_dip, 0.0_dip, 120.0_dip, 80.0_dip},
                     std::move(warp_options)) ||
        !warp.UsesSoftwareAdapter() || !warp.RenderFrame())
        return 7;
    warp.Destroy();

    D3DHost invalid;
    D3DHostOptions invalid_options;
    invalid_options.allow_hardware = false;
    invalid_options.allow_warp_fallback = false;
    if (invalid.Create(parent, {713}, {0.0_dip, 0.0_dip, 40.0_dip, 40.0_dip},
                       std::move(invalid_options)) || invalid.GetLastError() != E_INVALIDARG)
        return 8;

    D3DHost exceptional;
    D3DHostOptions exceptional_options;
    exceptional_options.vertical_sync = false;
    exceptional_options.callbacks.render = [](D3DFrameContext&) {
        throw std::runtime_error("render");
    };
    if (!exceptional.Create(parent, {714}, {0.0_dip, 0.0_dip, 80.0_dip, 60.0_dip},
                            std::move(exceptional_options)))
        return 9;
    const auto failed = exceptional.RenderFrame();
    if (failed.status != D3DFrameStatus::failed || failed.error != E_UNEXPECTED ||
        !exceptional.TakeCallbackException())
        return 10;
    exceptional.Destroy();

    D3DHost resizing;
    D3DHostOptions resizing_options;
    resizing_options.vertical_sync = false;
    bool resized_from_callback = false;
    resizing_options.callbacks.render = [&](D3DFrameContext&) {
        if (!resized_from_callback) {
            resized_from_callback = true;
            if (!resizing.SetBounds({0.0_dip, 0.0_dip, 130.0_dip, 90.0_dip}))
                throw std::runtime_error("reentrant resize");
        }
    };
    if (!resizing.Create(parent, {716}, {0.0_dip, 0.0_dip, 80.0_dip, 60.0_dip},
                         std::move(resizing_options)) ||
        !resizing.RenderFrame() || !resized_from_callback || !resizing.RenderFrame())
        return 11;
    DXGI_SWAP_CHAIN_DESC1 resized_description{};
    RECT resized_client{};
    if (FAILED(resizing.GetSwapChain()->GetDesc1(&resized_description)) ||
        !::GetClientRect(resizing.GetHwnd(), &resized_client) ||
        resized_description.Width != static_cast<UINT>(resized_client.right) ||
        resized_description.Height != static_cast<UINT>(resized_client.bottom))
        return 12;
    resizing.Destroy();

    D3DHost closing;
    D3DHostOptions closing_options;
    closing_options.vertical_sync = false;
    closing_options.callbacks.render = [&](D3DFrameContext&) { closing.Destroy(); };
    if (!closing.Create(parent, {715}, {0.0_dip, 0.0_dip, 80.0_dip, 60.0_dip},
                        std::move(closing_options)))
        return 13;
    const auto closed = closing.RenderFrame();
    if (closed.status != D3DFrameStatus::failed ||
        closed.error != HRESULT_FROM_WIN32(ERROR_CANCELLED) ||
        closing.GetRenderState() != D3DRenderState::destroyed)
        return 14;

    const DWORD resources_before = GuiResources();
    for (int iteration = 0; iteration < 24; ++iteration) {
        D3DHost transient;
        D3DHostOptions transient_options;
        transient_options.allow_hardware = false;
        transient_options.allow_warp_fallback = true;
        transient_options.vertical_sync = false;
        transient_options.callbacks.render = [](D3DFrameContext& frame) {
            const float color[] = {0, 0, 0, 1};
            ID3D11RenderTargetView* target = &frame.render_target;
            frame.context.ClearRenderTargetView(target, color);
        };
        if (!transient.Create(parent, {800 + iteration},
                              {0.0_dip, 0.0_dip, 32.0_dip, 32.0_dip},
                              std::move(transient_options)) ||
            !transient.RenderFrame())
            return 15;
        transient.Destroy();
    }
    if (GuiResources() > resources_before + 8) return 16;

    host.Destroy();
    if (host.GetRenderState() != D3DRenderState::destroyed || discards != 1) return 17;
    ::DestroyWindow(parent);
    return 0;
}
