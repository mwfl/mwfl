#include <mwtl/mwtl.h>
#include <mwtl/d2d_host.h>

#include "drawing_model.h"

#include <windowsx.h>

#include <filesystem>
#include <stdexcept>
#include <string>

using mwtl::operator""_dip;

namespace {
constexpr mwtl::ControlId kUndo{801};
constexpr mwtl::ControlId kClear{802};
constexpr mwtl::ControlId kExport{803};
constexpr UINT kRunSelfTest = WM_APP + 0x160;
bool g_self_test = false;

class DrawingWindow final : public mwtl::WindowBase {
public:
    void BuildUI() override {
        SetTitle(L"mwtl Drawing");
        mwtl::ControlHost ui{*this};
        ui.Add(undo_, kUndo, L"Undo (Ctrl+Z)", {0.0_dip, 0.0_dip, 120.0_dip, 32.0_dip});
        ui.Add(clear_, kClear, L"Clear (Delete)", {0.0_dip, 0.0_dip, 120.0_dip, 32.0_dip});
        ui.Add(export_, kExport, L"Export SVG", {0.0_dip, 0.0_dip, 120.0_dip, 32.0_dip});
        ui.Add(status_, L"Draw with mouse or touchpad; coordinates are DPI-independent.");

        mwtl::D2DHostOptions options;
        options.callbacks.create_device_resources = [this](ID2D1HwndRenderTarget& target) {
            const HRESULT result = target.CreateSolidColorBrush(
                D2D1::ColorF(D2D1::ColorF::RoyalBlue), brush_.ReleaseAndGetAddressOf());
            if (FAILED(result)) throw std::runtime_error("create drawing brush failed");
        };
        options.callbacks.discard_device_resources = [this] { brush_.Reset(); };
        options.callbacks.paint = [this](mwtl::D2DRenderContext& context) {
            if (!brush_) throw std::runtime_error("drawing brush is unavailable");
            for (const auto& stroke : document_.GetStrokes()) {
                if (stroke.size() == 1) {
                    const auto point = D2D1::Point2F(stroke.front().x, stroke.front().y);
                    context.target.FillEllipse(D2D1::Ellipse(point, 1.5f, 1.5f), brush_.Get());
                    continue;
                }
                for (std::size_t index = 1; index < stroke.size(); ++index) {
                    context.target.DrawLine(D2D1::Point2F(stroke[index - 1].x,
                                                          stroke[index - 1].y),
                                            D2D1::Point2F(stroke[index].x, stroke[index].y),
                                            brush_.Get(), 3.0f);
                }
            }
        };
        options.callbacks.input = [this](const mwtl::D2DInputEvent& event) {
            return HandleDrawingInput(event);
        };
        mwtl::Must(canvas_.Create(*this, {804}, {0.0_dip, 0.0_dip, 800.0_dip, 520.0_dip},
                                  std::move(options)),
                   "create Direct2D drawing surface");
        SetLayout(mwtl::Column()
                      .Margin(12.0_dip)
                      .Gap(8.0_dip)
                      .Add(mwtl::Row().Gap(8.0_dip)
                               .Add(undo_, mwtl::Auto())
                               .Add(clear_, mwtl::Auto())
                               .Add(export_, mwtl::Auto()),
                           mwtl::Auto())
                      .Add(canvas_, mwtl::Stretch())
                      .Add(status_, mwtl::Auto()));
        mwtl::ApplyWindowAppearance(GetHwnd());
        if (g_self_test && !::PostMessageW(GetHwnd(), kRunSelfTest, 0, 0))
            throw std::runtime_error("post drawing self-test failed");
    }

    mwtl::EventResult OnCommand(const mwtl::CommandEvent& event) override {
        if (event.IsClicked(undo_)) {
            if (document_.Undo()) canvas_.Invalidate();
            UpdateStatus();
            return mwtl::EventResult::Handled();
        }
        if (event.IsClicked(clear_)) {
            document_.Clear();
            canvas_.Invalidate();
            UpdateStatus();
            return mwtl::EventResult::Handled();
        }
        if (event.IsClicked(export_)) {
            ExportInteractive();
            return mwtl::EventResult::Handled();
        }
        return mwtl::EventResult::Propagate();
    }

    mwtl::EventResult OnMessage(const mwtl::WindowMessage& event) override {
        if (event.id == kRunSelfTest) {
            RunSelfTest();
            return mwtl::EventResult::Handled();
        }
        return mwtl::EventResult::Propagate();
    }

private:
    mwtl::EventResult HandleDrawingInput(const mwtl::D2DInputEvent& event) {
        const drawing::Point point{event.position.x.value, event.position.y.value};
        if (event.message == WM_LBUTTONDOWN) {
            document_.BeginStroke(point);
            ::SetCapture(canvas_.GetHwnd());
            canvas_.Invalidate();
            return mwtl::EventResult::Handled();
        }
        if (event.message == WM_MOUSEMOVE && (event.wparam & MK_LBUTTON) != 0) {
            document_.AppendPoint(point);
            canvas_.Invalidate();
            return mwtl::EventResult::Handled();
        }
        if (event.message == WM_LBUTTONUP) {
            document_.AppendPoint(point);
            document_.EndStroke();
            if (::GetCapture() == canvas_.GetHwnd()) ::ReleaseCapture();
            UpdateStatus();
            return mwtl::EventResult::Handled();
        }
        if (event.message == WM_KEYDOWN && event.wparam == VK_DELETE) {
            document_.Clear();
            canvas_.Invalidate();
            UpdateStatus();
            return mwtl::EventResult::Handled();
        }
        if (event.message == WM_KEYDOWN && event.wparam == 'Z' &&
            (::GetKeyState(VK_CONTROL) & 0x8000) != 0) {
            if (document_.Undo()) canvas_.Invalidate();
            UpdateStatus();
            return mwtl::EventResult::Handled();
        }
        return mwtl::EventResult::Propagate();
    }

    bool ExportTo(const std::filesystem::path& path) {
        RECT client{};
        ::GetClientRect(canvas_.GetHwnd(), &client);
        const auto dpi = mwtl::DpiContext::FromWindow(canvas_.GetHwnd());
        const auto result = mwtl::WriteTextFileAtomic(
            path, document_.ToSvg(dpi.FromPixels(client.right).value,
                                  dpi.FromPixels(client.bottom).value));
        return result.Succeeded();
    }

    void ExportInteractive() {
        const auto selected = mwtl::ShowSaveFileDialog({
            .owner = GetHwnd(), .title = L"Export drawing as SVG",
            .filters = {{L"SVG images", L"*.svg"}}, .default_extension = L"svg"});
        if (selected.accepted && ExportTo(selected.path)) status_.SetText(L"SVG exported.");
    }

    void UpdateStatus() {
        status_.SetText(L"Strokes: " + std::to_wstring(document_.GetStrokeCount()) +
                        L" | GPU resources may be recreated without losing data.");
    }

    void RunSelfTest() noexcept {
        int result = 0;
        try {
            const HWND canvas = canvas_.GetHwnd();
            ::SendMessageW(canvas, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(10, 12));
            ::SendMessageW(canvas, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(40, 44));
            ::SendMessageW(canvas, WM_LBUTTONUP, 0, MAKELPARAM(80, 70));
            if (document_.GetStrokeCount() != 1 || document_.IsDrawing() || !canvas_.Render())
                result = 1;
            canvas_.DiscardDeviceResources();
            if (result == 0 && (!canvas_.Render() || canvas_.GetRenderTarget() == nullptr)) result = 2;
            const auto path = std::filesystem::temp_directory_path() /
                              (L"mwtl-drawing-" + std::to_wstring(::GetCurrentProcessId()) + L".svg");
            if (result == 0 && !ExportTo(path)) result = 3;
            const auto exported = mwtl::ReadTextFile(path);
            if (result == 0 && (!exported.Succeeded() ||
                                exported.value->text.find(L"<polyline") == std::wstring::npos))
                result = 4;
            ::DeleteFileW(path.c_str());
            ::SendMessageW(clear_.GetHwnd(), BM_CLICK, 0, 0);
            if (result == 0 && document_.GetStrokeCount() != 0) result = 5;
        } catch (...) {
            result = 6;
        }
        ::PostQuitMessage(result);
    }

    drawing::Document document_;
    mwtl::Button undo_;
    mwtl::Button clear_;
    mwtl::Button export_;
    mwtl::Label status_;
    mwtl::D2DHost canvas_;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush_;
};
}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
    g_self_test = std::wstring_view{::GetCommandLineW()}.find(L"--self-test") !=
                  std::wstring_view::npos;
    return mwtl::RunApplication<DrawingWindow>(instance, show_command);
}
