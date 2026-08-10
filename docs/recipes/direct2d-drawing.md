# Add a DPI-aware Direct2D drawing surface

This recipe uses Visual Studio 2026, MSVC, C++20, and x64.

## 1. Link the optional component

For an installed package:

```cmake
find_package(mwtl CONFIG REQUIRED COMPONENTS d2d)
target_link_libraries(my_app PRIVATE mwtl::d2d)
```

For `add_subdirectory`/`FetchContent`, link the same `mwtl::d2d` target. Do not
add `d2d1` to `mwtl::mwtl`; projects that do not request drawing should not
inherit the dependency.

## 2. Store the host and resources

```cpp
#include <mwtl/d2d_host.h>

mwtl::D2DHost canvas_;
Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush_;
```

Both belong to the window's UI thread. Keep strokes or other application data
in a separate model that contains DIPs, not COM pointers.

## 3. Define resource and paint callbacks

```cpp
mwtl::D2DHostOptions options;
options.callbacks.create_device_resources = [this](ID2D1HwndRenderTarget& target) {
    mwtl::Must(SUCCEEDED(target.CreateSolidColorBrush(
        D2D1::ColorF(D2D1::ColorF::RoyalBlue), brush_.ReleaseAndGetAddressOf())),
        "create brush");
};
options.callbacks.discard_device_resources = [this] { brush_.Reset(); };
options.callbacks.paint = [this](mwtl::D2DRenderContext& context) {
    context.target.DrawLine(D2D1::Point2F(10, 10), D2D1::Point2F(100, 80),
                            brush_.Get(), 3.0f);
};
```

Do not call `BeginDraw` or `EndDraw`: `D2DHost` owns that transaction. Do not
retain the callback context or target. If the target is lost, discard runs and
the next paint recreates resources before drawing.

## 4. Create and lay out the host

```cpp
mwtl::Must(canvas_.Create(*this, {500},
                          {0.0_dip, 0.0_dip, 640.0_dip, 480.0_dip},
                          std::move(options)),
           "create drawing surface");
SetLayout(mwtl::Column().Margin(12.0_dip).Add(canvas_, mwtl::Stretch()));
```

`D2DHost` reacts to resize, parent DPI changes, theme changes, and High
Contrast. Mouse input positions supplied through `callbacks.input` are DIPs.
Use `Invalidate()` after the application model changes.

## 5. Verify recovery

The `examples/drawing` self-test draws a stroke, renders it, explicitly discards
device resources, renders again, exports SVG, reads the file back, and activates
the Clear button. Run the same test locally:

```powershell
ctest --test-dir build/presets/vs2026-x64 -C Debug -R mwtl.drawing_gui
```
