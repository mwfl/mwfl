#include <mwfl/mwfl.h>
#include <mwfl/d2d_host.h>
#include <mwfl/imaging.h>

#include <windowsx.h>
#include <wil/resource.h>

#include <array>
#include <cmath>
#include <filesystem>
#include <stdexcept>
#include <string>

using mwfl::operator""_dip;

namespace {
constexpr mwfl::ControlId kOpen{821};
constexpr mwfl::ControlId kFit{822};
constexpr mwfl::ControlId kActual{823};
constexpr mwfl::ControlId kZoomIn{824};
constexpr mwfl::ControlId kZoomOut{825};
constexpr UINT kRunSelfTest = WM_APP + 0x170;
bool g_self_test = false;

D2D1_COLOR_F SystemColor(int index) {
    const COLORREF color = ::GetSysColor(index);
    return D2D1::ColorF(GetRValue(color) / 255.0f, GetGValue(color) / 255.0f,
                        GetBValue(color) / 255.0f, 1.0f);
}

mwfl::DecodedImage MakeWelcomeImage() {
    mwfl::DecodedImage image;
    image.width = 640;
    image.height = 400;
    image.stride = image.width * 4;
    image.pixels.resize(static_cast<std::size_t>(image.stride) * image.height);
    for (std::uint32_t y = 0; y < image.height; ++y) {
        for (std::uint32_t x = 0; x < image.width; ++x) {
            auto* pixel = reinterpret_cast<std::uint8_t*>(image.pixels.data()) +
                          static_cast<std::size_t>(y) * image.stride + x * 4;
            pixel[0] = static_cast<std::uint8_t>(40 + x * 150 / image.width);
            pixel[1] = static_cast<std::uint8_t>(60 + y * 150 / image.height);
            pixel[2] = static_cast<std::uint8_t>(180 + x * 60 / image.width);
            pixel[3] = 255;
        }
    }
    return image;
}

bool WriteSelfTestBmp(const std::filesystem::path& path) {
    BITMAPFILEHEADER file{};
    BITMAPINFOHEADER info{};
    constexpr std::array<BYTE, 24> pixels{
        0, 0, 255, 0, 255, 0, 255, 0, 0, 0, 0, 0,
        255, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    file.bfType = 0x4D42;
    file.bfOffBits = sizeof(file) + sizeof(info);
    file.bfSize = file.bfOffBits + static_cast<DWORD>(pixels.size());
    info.biSize = sizeof(info);
    info.biWidth = 3;
    info.biHeight = 2;
    info.biPlanes = 1;
    info.biBitCount = 24;
    info.biCompression = BI_RGB;
    info.biSizeImage = static_cast<DWORD>(pixels.size());
    wil::unique_hfile handle(::CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                           FILE_ATTRIBUTE_TEMPORARY, nullptr));
    DWORD written = 0;
    return handle && ::WriteFile(handle.get(), &file, sizeof(file), &written, nullptr) &&
           written == sizeof(file) &&
           ::WriteFile(handle.get(), &info, sizeof(info), &written, nullptr) &&
           written == sizeof(info) &&
           ::WriteFile(handle.get(), pixels.data(), static_cast<DWORD>(pixels.size()), &written,
                       nullptr) &&
           written == pixels.size();
}

class ImageViewerWindow final : public mwfl::WindowBase {
public:
    void BuildUI() override {
        SetTitle(L"mwfl Image Viewer");
        image_ = MakeWelcomeImage();
        viewport_.SetImageSize(image_.width, image_.height);
        viewport_.SetViewport({900.0_dip, 560.0_dip});
        viewport_.Fit();

        mwfl::ControlHost ui{*this};
        ui.Add(open_, kOpen, L"Open...", {0.0_dip, 0.0_dip, 90.0_dip, 32.0_dip});
        ui.Add(fit_, kFit, L"Fit (F)", {0.0_dip, 0.0_dip, 80.0_dip, 32.0_dip});
        ui.Add(actual_, kActual, L"100% (0)", {0.0_dip, 0.0_dip, 90.0_dip, 32.0_dip});
        ui.Add(zoom_in_, kZoomIn, L"Zoom +", {0.0_dip, 0.0_dip, 80.0_dip, 32.0_dip});
        ui.Add(zoom_out_, kZoomOut, L"Zoom -", {0.0_dip, 0.0_dip, 80.0_dip, 32.0_dip});
        ui.Add(status_, L"Built-in image | Fit | drag to pan, wheel to zoom");

        mwfl::D2DHostOptions options;
        options.callbacks.create_device_resources = [this](ID2D1HwndRenderTarget& target) {
            CreateDeviceResources(target);
        };
        options.callbacks.discard_device_resources = [this] {
            bitmap_.Reset();
            background_a_.Reset();
            background_b_.Reset();
        };
        options.callbacks.paint = [this](mwfl::D2DRenderContext& context) { Paint(context); };
        options.callbacks.input = [this](const mwfl::D2DInputEvent& event) {
            return HandleInput(event);
        };
        mwfl::Must(canvas_.Create(*this, {826}, {0.0_dip, 0.0_dip, 900.0_dip, 560.0_dip},
                                  std::move(options)),
                   "create image canvas");
        mwfl::Must(mwfl::SetAccessibleName(canvas_.GetHwnd(), L"Image viewport"),
                   "name image viewport");
        SetLayout(mwfl::Column()
                      .Margin(10.0_dip)
                      .Gap(8.0_dip)
                      .Add(mwfl::Row().Gap(6.0_dip)
                               .Add(open_, mwfl::Auto())
                               .Add(fit_, mwfl::Auto())
                               .Add(actual_, mwfl::Auto())
                               .Add(zoom_in_, mwfl::Auto())
                               .Add(zoom_out_, mwfl::Auto()),
                           mwfl::Auto())
                      .Add(canvas_, mwfl::Stretch())
                      .Add(status_, mwfl::Auto()));
        mwfl::ApplyWindowAppearance(GetHwnd());
        if (g_self_test && !::PostMessageW(GetHwnd(), kRunSelfTest, 0, 0))
            throw std::runtime_error("post image-viewer self-test failed");
    }

    mwfl::EventResult OnCommand(const mwfl::CommandEvent& event) override {
        if (event.IsClicked(open_)) {
            OpenInteractive();
            return mwfl::EventResult::Handled();
        }
        if (event.IsClicked(fit_)) {
            viewport_.Fit();
            Refresh(L"Fit");
            return mwfl::EventResult::Handled();
        }
        if (event.IsClicked(actual_)) {
            viewport_.SetZoom(1, Center());
            Refresh(L"Actual size");
            return mwfl::EventResult::Handled();
        }
        if (event.IsClicked(zoom_in_)) {
            viewport_.ZoomBy(1.25f, Center());
            Refresh(L"Zoom in");
            return mwfl::EventResult::Handled();
        }
        if (event.IsClicked(zoom_out_)) {
            viewport_.ZoomBy(0.8f, Center());
            Refresh(L"Zoom out");
            return mwfl::EventResult::Handled();
        }
        return mwfl::EventResult::Propagate();
    }

    mwfl::EventResult OnMessage(const mwfl::WindowMessage& event) override {
        if (event.id == WM_THEMECHANGED || event.id == WM_SYSCOLORCHANGE ||
            event.id == WM_SETTINGCHANGE) {
            mwfl::ApplyWindowAppearance(GetHwnd());
            canvas_.DiscardDeviceResources();
            canvas_.Invalidate();
            return mwfl::EventResult::Handled();
        }
        if (event.id == kRunSelfTest) {
            RunSelfTest();
            return mwfl::EventResult::Handled();
        }
        return mwfl::EventResult::Propagate();
    }

private:
    void CreateDeviceResources(ID2D1HwndRenderTarget& target) {
        if (!image_) throw std::runtime_error("decoded image is empty");
        const auto properties = D2D1::BitmapProperties(
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
            96, 96);
        HRESULT result = target.CreateBitmap(D2D1::SizeU(image_.width, image_.height),
                                             image_.pixels.data(), image_.stride, properties,
                                             bitmap_.ReleaseAndGetAddressOf());
        if (SUCCEEDED(result))
            result = target.CreateSolidColorBrush(SystemColor(COLOR_APPWORKSPACE),
                                                  background_a_.ReleaseAndGetAddressOf());
        if (SUCCEEDED(result))
            result = target.CreateSolidColorBrush(SystemColor(COLOR_WINDOW),
                                                  background_b_.ReleaseAndGetAddressOf());
        if (FAILED(result)) throw std::runtime_error("create image-viewer resources failed");
    }

    void Paint(mwfl::D2DRenderContext& context) {
        viewport_.SetViewport({mwfl::Dip(context.size_dip.width),
                               mwfl::Dip(context.size_dip.height)});
        if (!bitmap_ || !background_a_ || !background_b_) return;
        constexpr float tile = 24;
        context.target.FillRectangle(D2D1::RectF(0, 0, context.size_dip.width,
                                                  context.size_dip.height),
                                     background_a_.Get());
        for (float y = 0; y < context.size_dip.height; y += tile) {
            for (float x = 0; x < context.size_dip.width; x += tile) {
                if ((static_cast<int>(x / tile) + static_cast<int>(y / tile)) % 2 == 0)
                    context.target.FillRectangle(D2D1::RectF(x, y, x + tile, y + tile),
                                                 background_b_.Get());
            }
        }
        const auto transform = viewport_.GetTransform();
        const D2D1_RECT_F destination = D2D1::RectF(
            transform.origin.x.value, transform.origin.y.value,
            transform.origin.x.value + transform.rendered_size.width.value,
            transform.origin.y.value + transform.rendered_size.height.value);
        const D2D1_RECT_F source = D2D1::RectF(0, 0, static_cast<float>(image_.width),
                                               static_cast<float>(image_.height));
        context.target.DrawBitmap(bitmap_.Get(), destination, 1.0f,
                                  transform.scale >= 8 ? D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR
                                                       : D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
                                  source);
    }

    mwfl::EventResult HandleInput(const mwfl::D2DInputEvent& event) {
        if (event.message == WM_MOUSEWHEEL) {
            viewport_.ZoomBy(GET_WHEEL_DELTA_WPARAM(event.wparam) > 0 ? 1.2f : 1.0f / 1.2f,
                             event.position);
            Refresh(L"Wheel zoom");
            return mwfl::EventResult::Handled();
        }
        if (event.message == WM_LBUTTONDOWN) {
            dragging_ = true;
            drag_point_ = event.position;
            ::SetCapture(canvas_.GetHwnd());
            return mwfl::EventResult::Handled();
        }
        if (event.message == WM_MOUSEMOVE && dragging_) {
            viewport_.PanBy({mwfl::Dip(event.position.x.value - drag_point_.x.value),
                             mwfl::Dip(event.position.y.value - drag_point_.y.value)});
            drag_point_ = event.position;
            canvas_.Invalidate();
            return mwfl::EventResult::Handled();
        }
        if (event.message == WM_LBUTTONUP && dragging_) {
            dragging_ = false;
            if (::GetCapture() == canvas_.GetHwnd()) ::ReleaseCapture();
            Refresh(L"Pan");
            return mwfl::EventResult::Handled();
        }
        if (event.message == WM_KEYDOWN && event.wparam == 'F') {
            viewport_.Fit(); Refresh(L"Fit"); return mwfl::EventResult::Handled();
        }
        if (event.message == WM_KEYDOWN && event.wparam == '0') {
            viewport_.SetZoom(1, Center()); Refresh(L"Actual size");
            return mwfl::EventResult::Handled();
        }
        return mwfl::EventResult::Propagate();
    }

    mwfl::PointDip Center() const {
        const auto size = viewport_.GetViewport();
        return {mwfl::Dip(size.width.value * 0.5f), mwfl::Dip(size.height.value * 0.5f)};
    }

    bool Load(const std::filesystem::path& path) {
        auto decoded = mwfl::DecodeImageFile(path);
        if (!decoded) return false;
        image_ = std::move(decoded.image);
        viewport_.SetImageSize(image_.width, image_.height);
        viewport_.Fit();
        canvas_.DiscardDeviceResources();
        Refresh(image_.color_managed_to_srgb ? L"Loaded | color-managed to sRGB"
                                             : L"Loaded | normalized BGRA8");
        return true;
    }

    void OpenInteractive() {
        const auto selected = mwfl::ShowOpenFileDialog({
            .owner = GetHwnd(), .title = L"Open image",
            .filters = {{L"Images", L"*.png;*.jpg;*.jpeg;*.bmp;*.gif;*.tif;*.tiff"},
                        {L"All files", L"*.*"}}});
        if (selected.accepted && !Load(selected.path)) status_.SetText(L"Image could not be decoded.");
    }

    void Refresh(std::wstring_view action) {
        status_.SetText(std::wstring(action) + L" | " + std::to_wstring(image_.width) + L" x " +
                        std::to_wstring(image_.height) + L" | " +
                        std::to_wstring(static_cast<int>(std::lround(viewport_.GetZoom() * 100))) +
                        L"%");
        canvas_.Invalidate();
    }

    void RunSelfTest() noexcept {
        int result = 0;
        const auto path = std::filesystem::temp_directory_path() /
                          (L"mwfl-viewer-" + std::to_wstring(::GetCurrentProcessId()) + L".bmp");
        try {
            if (!WriteSelfTestBmp(path) || !Load(path) || image_.width != 3 || image_.height != 2)
                result = 1;
            if (result == 0 && !canvas_.Render()) result = 2;
            const float before_zoom = viewport_.GetZoom();
            POINT wheel{30, 30};
            ::ClientToScreen(canvas_.GetHwnd(), &wheel);
            ::SendMessageW(canvas_.GetHwnd(), WM_MOUSEWHEEL, MAKEWPARAM(0, WHEEL_DELTA),
                           MAKELPARAM(wheel.x, wheel.y));
            if (result == 0 && viewport_.GetZoom() <= before_zoom) result = 3;
            ::SendMessageW(canvas_.GetHwnd(), WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(20, 20));
            ::SendMessageW(canvas_.GetHwnd(), WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(40, 35));
            ::SendMessageW(canvas_.GetHwnd(), WM_LBUTTONUP, 0, MAKELPARAM(40, 35));
            canvas_.DiscardDeviceResources();
            if (result == 0 && (!canvas_.Render() || canvas_.GetRenderTarget() == nullptr)) result = 4;
            ::SendMessageW(fit_.GetHwnd(), BM_CLICK, 0, 0);
            if (result == 0 && viewport_.GetZoom() <= 0) result = 5;
        } catch (...) {
            result = 6;
        }
        ::DeleteFileW(path.c_str());
        ::PostQuitMessage(result);
    }

    mwfl::DecodedImage image_;
    mwfl::ImageViewportModel viewport_;
    mwfl::Button open_;
    mwfl::Button fit_;
    mwfl::Button actual_;
    mwfl::Button zoom_in_;
    mwfl::Button zoom_out_;
    mwfl::Label status_;
    mwfl::D2DHost canvas_;
    Microsoft::WRL::ComPtr<ID2D1Bitmap> bitmap_;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> background_a_;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> background_b_;
    bool dragging_ = false;
    mwfl::PointDip drag_point_{};
};
}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
    g_self_test = std::wstring_view{::GetCommandLineW()}.find(L"--self-test") !=
                  std::wstring_view::npos;
    return mwfl::RunApplication<ImageViewerWindow>(
        instance, show_command, {}, {.com_apartment = mwfl::ComApartment::sta});
}
