# Build and extend the Image Viewer

This tutorial assumes Windows 10 or later, Visual Studio 2026 with Desktop
development with C++, CMake, and the Windows SDK. The repository uses C++20 and
x64 for the primary local workflow.

## Build the existing application

From a Developer PowerShell in the repository root:

```powershell
cmake --preset vs2026-x64
cmake --build --preset vs2026-x64-debug --target mwfl_image_viewer_demo
./build/presets/vs2026-x64/examples/image_viewer/Debug/mwfl_image_viewer_demo.exe
```

Select **Open...**, choose PNG, JPEG, BMP, GIF, or TIFF, then use the mouse
wheel to zoom and drag the image to pan. **Fit (F)** and **100% (0)** are also
keyboard accessible through native buttons and canvas shortcuts.

## Understand the three layers

1. `DecodeImageFile` uses WIC and creates bounded CPU-owned `DecodedImage`
   pixels. The default decoded budget is 256 MiB.
2. `ImageViewportModel` calculates Fit, anchored zoom, and clamped pan without
   HWNDs or a GPU.
3. `D2DHost` creates an `ID2D1Bitmap` cache from the CPU pixels. Theme, High
   Contrast, target loss, or an explicit discard recreates the cache without
   decoding again or losing content.

Installed applications request the optional components explicitly:

```cmake
find_package(mwfl CONFIG REQUIRED COMPONENTS imaging d2d)
target_link_libraries(my_viewer PRIVATE mwfl::imaging mwfl::d2d)
```

Create the application with an STA because WIC is COM-based:

```cpp
return mwfl::RunApplication<MyViewer>(
    instance, show, {}, {.com_apartment = mwfl::ComApartment::sta});
```

## Handle decode results

Never infer failure from an empty pixel vector alone. Inspect
`ImageDecodeStatus`, especially `too_large`, and show a useful message. Raising
`maximum_pixels` is an application decision with a predictable four bytes per
normalized pixel. `has_embedded_color_profile` does not mean conversion
succeeded; only `color_managed_to_srgb` makes that claim.

## Test without dialogs

```powershell
ctest --test-dir build/presets/vs2026-x64 -C Debug `
  -R "mwfl.(imaging_model|imaging_decode|image_viewer_gui|d2d_host_native)"
```

The GUI self-test creates a local BMP, decodes it, renders, zooms at a wheel
anchor, pans, discards and recreates GPU resources, activates Fit, deletes the
temporary file, and exits. It needs no network or user input.
