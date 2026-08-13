# Render an on-demand Direct3D frame

This recipe uses Visual Studio 2026, MSVC, C++20, and x64.

## 1. Request the component

```cmake
find_package(mwfl CONFIG REQUIRED COMPONENTS d3d)
target_link_libraries(my_app PRIVATE mwfl::d3d)
```

The core `mwfl::ui` target deliberately exposes neither `d3d11` nor `dxgi`.

## 2. Configure rendering

```cpp
mwfl::D3DHostOptions options;
options.allow_hardware = true;
options.allow_warp_fallback = true;
options.vertical_sync = true;
options.callbacks.render = [](mwfl::D3DFrameContext& frame) {
    const float color[] = {0.08f, 0.25f, 0.55f, 1.0f};
    ID3D11RenderTargetView* target = &frame.render_target;
    frame.context.OMSetRenderTargets(1, &target, nullptr);
    frame.context.ClearRenderTargetView(target, color);
};
mwfl::Must(surface_.Create(*this, {600},
                           {0.0_dip, 0.0_dip, 640.0_dip, 480.0_dip},
                           std::move(options)),
           "create D3D surface");
```

The swap chain is flip-model BGRA8 UNORM with ignored alpha. The context, RTV,
device, and swap chain are borrowed UI-thread views. Do not retain them.

## 3. Render only when needed

```cpp
const mwfl::D3DFrameResult result = surface_.RenderFrame();
switch (result.status) {
case mwfl::D3DFrameStatus::presented: break;
case mwfl::D3DFrameStatus::minimized: break; // successful no-op
case mwfl::D3DFrameStatus::occluded: break;  // wait for later activity
case mwfl::D3DFrameStatus::device_recreated:
    surface_.Invalidate();                    // requested frame was not presented
    break;
case mwfl::D3DFrameStatus::failed:
    HandleRenderFailure(result.error);
    break;
}
```

`D3DHost` also renders on `WM_PAINT`; it never installs a timer. If the
application genuinely needs animation, schedule invalidation in application
code and stop when minimized or occluded.

## 4. Own resources at the right layer

Create shaders, textures, and states in `create_device_resources`; release them
in `discard_device_resources`. Keep documents, decoded images, and other
authoritative content outside GPU objects. Device removal then recreates the
device and callbacks without losing user data. Resize requested reentrantly
from a render callback is deferred until that frame ends.
