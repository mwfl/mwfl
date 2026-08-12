#include <mwfl/mwfl.h>
#include <mwfl/printing_settings.h>

#include <algorithm>
#include <array>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

using mwfl::operator""_dip;

namespace {
constexpr mwfl::ControlId kPrevious{841};
constexpr mwfl::ControlId kNext{842};
constexpr mwfl::ControlId kZoomOut{843};
constexpr mwfl::ControlId kZoomIn{844};
constexpr mwfl::ControlId kSettings{845};
constexpr mwfl::ControlId kPrint{846};
constexpr UINT kRunSelfTest = WM_APP + 0x1B0;
bool g_self_test = false;

std::vector<std::wstring> MakeDocument() {
    std::vector<std::wstring> lines;
    lines.reserve(83);
    lines.push_back(L"mwfl printing and preview reference");
    lines.push_back(L"The same PrintPage range renders on screen and on the printer DC.");
    lines.push_back(L"");
    for (int index = 1; index <= 80; ++index)
        lines.push_back(L"Document line " + std::to_wstring(index) +
                        L" — Unicode: 中文, Ελληνικά, café");
    return lines;
}

bool RenderPage(HDC dc, RECT bounds, const mwfl::PrintPage& page,
                const std::vector<std::wstring>& lines, bool preview) {
    if (!dc || bounds.right <= bounds.left || bounds.bottom <= bounds.top ||
        page.content_begin < 0 || page.content_end < page.content_begin)
        return false;
    const COLORREF background = preview ? ::GetSysColor(COLOR_WINDOW) : RGB(255, 255, 255);
    const COLORREF foreground = preview ? ::GetSysColor(COLOR_WINDOWTEXT) : RGB(0, 0, 0);
    HBRUSH brush = ::CreateSolidBrush(background);
    if (!brush) return false;
    ::FillRect(dc, &bounds, brush);
    ::DeleteObject(brush);
    ::SetBkMode(dc, TRANSPARENT);
    ::SetTextColor(dc, foreground);
    const int width = bounds.right - bounds.left;
    const int height = bounds.bottom - bounds.top;
    const int margin_x = (std::max)(12, width / 18);
    const int margin_y = (std::max)(12, height / 24);
    RECT text{bounds.left + margin_x, bounds.top + margin_y,
              bounds.right - margin_x, bounds.bottom - margin_y};
    const int line_height = (std::max)(16, static_cast<int>((text.bottom - text.top) / 36));
    int y = text.top;
    for (std::int64_t index = page.content_begin; index < page.content_end; ++index) {
        if (index < 0 || static_cast<std::size_t>(index) >= lines.size()) return false;
        RECT line{text.left, y, text.right, y + line_height};
        ::DrawTextW(dc, lines[static_cast<std::size_t>(index)].c_str(), -1, &line,
                    DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        y += line_height;
    }
    const std::wstring footer = L"Page " + std::to_wstring(page.index + 1);
    RECT footer_bounds{text.left, text.bottom - line_height, text.right, text.bottom};
    ::DrawTextW(dc, footer.c_str(), -1, &footer_bounds,
                DT_RIGHT | DT_BOTTOM | DT_SINGLELINE | DT_NOPREFIX);
    return true;
}

class PrintingWindow final : public mwfl::WindowBase {
public:
    void BuildUI() override {
        SetTitle(L"mwfl Print Preview");
        ::SetWindowPos(GetHwnd(), nullptr, 0, 0, 1000, 820,
                       SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        lines_ = MakeDocument();
        pages_ = mwfl::PaginateContent(static_cast<std::int64_t>(lines_.size()), 32);
        preview_.SetPageCount(pages_.size());

        mwfl::ControlHost ui{*this};
        ui.Add(previous_, kPrevious, L"Previous (Page Up)", {0.0_dip, 0.0_dip, 130.0_dip, 32.0_dip});
        ui.Add(next_, kNext, L"Next (Page Down)", {0.0_dip, 0.0_dip, 120.0_dip, 32.0_dip});
        ui.Add(zoom_out_, kZoomOut, L"Zoom -", {0.0_dip, 0.0_dip, 80.0_dip, 32.0_dip});
        ui.Add(zoom_in_, kZoomIn, L"Zoom +", {0.0_dip, 0.0_dip, 80.0_dip, 32.0_dip});
        ui.Add(settings_button_, kSettings, L"Printer settings...",
               {0.0_dip, 0.0_dip, 135.0_dip, 32.0_dip});
        ui.Add(print_, kPrint, L"Print", {0.0_dip, 0.0_dip, 80.0_dip, 32.0_dip});
        ui.Add(status_, L"");
        SetLayout(mwfl::Column()
                      .Margin(10.0_dip)
                      .Gap(8.0_dip)
                      .Add(mwfl::Row().Gap(6.0_dip)
                               .Add(previous_, mwfl::Auto())
                               .Add(next_, mwfl::Auto())
                               .Add(zoom_out_, mwfl::Auto())
                               .Add(zoom_in_, mwfl::Auto())
                               .Add(settings_button_, mwfl::Auto())
                               .Add(print_, mwfl::Auto()),
                           mwfl::Auto())
                      .Add(status_, mwfl::Auto()));
        mwfl::Must(mwfl::SetAccessibleName(GetHwnd(), L"Document print preview"),
                   "name print preview window");
        SetAppearance({});
        const auto printers = mwfl::EnumerateLocalPrinters();
        const std::wstring discovery = printers
            ? L" | " + std::to_wstring(printers.printers.size()) + L" printer(s) discovered"
            : L" | printer discovery unavailable";
        UpdateStatus(L"Preview uses deterministic application pagination" + discovery);
        if (g_self_test && !::PostMessageW(GetHwnd(), kRunSelfTest, 0, 0))
            throw std::runtime_error("post printing self-test failed");
    }

    mwfl::EventResult OnCommand(const mwfl::CommandEvent& event) override {
        if (event.IsClicked(previous_)) { MovePage(-1); return mwfl::EventResult::Handled(); }
        if (event.IsClicked(next_)) { MovePage(1); return mwfl::EventResult::Handled(); }
        if (event.IsClicked(zoom_out_)) { Zoom(0.8); return mwfl::EventResult::Handled(); }
        if (event.IsClicked(zoom_in_)) { Zoom(1.25); return mwfl::EventResult::Handled(); }
        if (event.IsClicked(settings_button_)) {
            ConfigurePrinter();
            return mwfl::EventResult::Handled();
        }
        if (event.IsClicked(print_)) { PrintDocument(); return mwfl::EventResult::Handled(); }
        return mwfl::EventResult::Propagate();
    }

    mwfl::EventResult OnKeyDown(const mwfl::KeyEvent& event) override {
        if (event.virtual_key == VK_PRIOR) { MovePage(-1); return mwfl::EventResult::Handled(); }
        if (event.virtual_key == VK_NEXT) { MovePage(1); return mwfl::EventResult::Handled(); }
        if (event.virtual_key == VK_HOME && preview_.SelectPage(0)) {
            Refresh(L"First page"); return mwfl::EventResult::Handled();
        }
        if (event.virtual_key == VK_END && !pages_.empty() &&
            preview_.SelectPage(pages_.size() - 1)) {
            Refresh(L"Last page"); return mwfl::EventResult::Handled();
        }
        if (event.virtual_key == VK_ADD || event.virtual_key == VK_OEM_PLUS) {
            Zoom(1.25); return mwfl::EventResult::Handled();
        }
        if (event.virtual_key == VK_SUBTRACT || event.virtual_key == VK_OEM_MINUS) {
            Zoom(0.8); return mwfl::EventResult::Handled();
        }
        return mwfl::EventResult::Propagate();
    }

    mwfl::EventResult OnPaint(mwfl::PaintEvent& event) override {
        RECT client{};
        ::GetClientRect(GetHwnd(), &client);
        const auto dpi = mwfl::DpiContext::FromWindow(GetHwnd());
        const int top = dpi.ToPixels(82.0_dip);
        const int bottom = dpi.ToPixels(38.0_dip);
        RECT viewport{client.left + 12, client.top + top, client.right - 12,
                      client.bottom - bottom};
        DrawPreview(event.GetDC(), viewport);
        return mwfl::EventResult::Handled();
    }

    mwfl::EventResult OnMessage(const mwfl::WindowMessage& event) override {
        if (event.id == WM_THEMECHANGED || event.id == WM_SYSCOLORCHANGE ||
            event.id == WM_SETTINGCHANGE) {
            ::InvalidateRect(GetHwnd(), nullptr, TRUE);
            return mwfl::EventResult::Handled();
        }
        if (event.id == kRunSelfTest) {
            RunSelfTest();
            return mwfl::EventResult::Handled();
        }
        return mwfl::EventResult::Propagate();
    }

private:
    void MovePage(std::ptrdiff_t delta) {
        if (preview_.MoveBy(delta)) Refresh(delta < 0 ? L"Previous page" : L"Next page");
    }

    void Zoom(double factor) {
        if (preview_.SetZoom(preview_.GetZoom() * factor)) Refresh(L"Custom zoom");
    }

    void Refresh(std::wstring_view message) {
        UpdateStatus(message);
        ::InvalidateRect(GetHwnd(), nullptr, TRUE);
    }

    void UpdateStatus(std::wstring_view message) {
        std::wstring text(message);
        text += L" | Page " + std::to_wstring(preview_.GetSelectedPage() + 1) + L"/" +
                std::to_wstring(preview_.GetPageCount()) + L" | " +
                std::to_wstring(static_cast<int>(preview_.GetZoom() * 100)) + L"%";
        if (settings_.IsValid()) text += L" | " + settings_.PrinterName();
        else text += L" | No printer required for preview";
        status_.SetText(text);
    }

    RECT PreviewPageBounds(RECT viewport) const {
        const int available_width =
            (std::max)(1, static_cast<int>(viewport.right - viewport.left));
        const int available_height =
            (std::max)(1, static_cast<int>(viewport.bottom - viewport.top));
        const double paper_ratio = 210.0 / 297.0;
        int height = static_cast<int>(available_height * 0.90 * preview_.GetZoom());
        int width = static_cast<int>(height * paper_ratio);
        if (width > static_cast<int>(available_width * 0.90 * preview_.GetZoom())) {
            width = static_cast<int>(available_width * 0.90 * preview_.GetZoom());
            height = static_cast<int>(width / paper_ratio);
        }
        width = (std::max)(1, width);
        height = (std::max)(1, height);
        const int center_x = viewport.left + available_width / 2;
        const int center_y = viewport.top + available_height / 2;
        return {center_x - width / 2, center_y - height / 2,
                center_x + (width + 1) / 2, center_y + (height + 1) / 2};
    }

    bool DrawPreview(HDC dc, RECT viewport) const {
        if (!dc || pages_.empty()) return false;
        HBRUSH workspace = ::CreateSolidBrush(::GetSysColor(COLOR_APPWORKSPACE));
        if (!workspace) return false;
        ::FillRect(dc, &viewport, workspace);
        ::DeleteObject(workspace);
        RECT page_bounds = PreviewPageBounds(viewport);
        RECT shadow = page_bounds;
        ::OffsetRect(&shadow, 5, 5);
        HBRUSH shadow_brush = ::CreateSolidBrush(RGB(64, 64, 64));
        if (shadow_brush) { ::FillRect(dc, &shadow, shadow_brush); ::DeleteObject(shadow_brush); }
        return RenderPage(dc, page_bounds, pages_[preview_.GetSelectedPage()], lines_, true);
    }

    bool EnsureSettings() {
        if (settings_.IsValid()) return true;
        const auto default_printer = mwfl::QueryDefaultPrinter();
        if (!default_printer) {
            UpdateStatus(L"No default printer is available");
            return false;
        }
        auto loaded = mwfl::LoadPrinterSettings(default_printer.name);
        if (!loaded) {
            UpdateStatus(L"Printer settings could not be loaded");
            return false;
        }
        settings_ = std::move(loaded.settings);
        return true;
    }

    void ConfigurePrinter() {
        mwfl::PrinterSettings initial;
        if (EnsureSettings()) initial = settings_.Clone();
        auto result = mwfl::ShowPrintDialog(GetHwnd(), std::move(initial));
        if (result.action == mwfl::PrintDialogAction::accepted) {
            settings_ = std::move(result.settings);
            Refresh(L"Printer settings updated");
        } else if (result.action == mwfl::PrintDialogAction::cancelled) {
            UpdateStatus(L"Printer settings cancelled");
        } else {
            UpdateStatus(L"Printer dialog failed");
        }
    }

    void PrintDocument() {
        if (!EnsureSettings()) return;
        auto backend = mwfl::CreatePrinterBackend(settings_);
        if (!backend) { UpdateStatus(L"Printer device could not be opened"); return; }
        mwfl::PrintJob job(std::move(backend.backend));
        const auto result = mwfl::PrintPages(job, L"mwfl printing reference", pages_,
            [this](HDC dc, const mwfl::PrintPage& page) {
                RECT bounds{0, 0, ::GetDeviceCaps(dc, HORZRES), ::GetDeviceCaps(dc, VERTRES)};
                return RenderPage(dc, bounds, page, lines_, false);
            }, [](const mwfl::PrintPage&) {
                return (::GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
            });
        if (result == mwfl::PrintOperationStatus::success) UpdateStatus(L"Document printed");
        else if (result == mwfl::PrintOperationStatus::cancelled)
            UpdateStatus(L"Printing cancelled safely (Esc)");
        else UpdateStatus(L"Printing failed");
    }

    void RunSelfTest() noexcept {
        int result = 0;
        try {
            if (pages_.size() != 3 || preview_.GetSelectedPage() != 0) result = 1;
            ::SendMessageW(next_.GetHwnd(), BM_CLICK, 0, 0);
            ::SendMessageW(zoom_in_.GetHwnd(), BM_CLICK, 0, 0);
            if (result == 0 && (preview_.GetSelectedPage() != 1 ||
                                preview_.GetZoom() <= 1.0)) result = 2;
            HDC screen = ::GetDC(nullptr);
            HDC memory = screen ? ::CreateCompatibleDC(screen) : nullptr;
            HBITMAP bitmap = screen ? ::CreateCompatibleBitmap(screen, 640, 720) : nullptr;
            HGDIOBJ old = memory && bitmap ? ::SelectObject(memory, bitmap) : nullptr;
            if (result == 0 && (!memory || !bitmap || !DrawPreview(memory, {0, 0, 640, 720})))
                result = 3;
            if (old) ::SelectObject(memory, old);
            if (bitmap) ::DeleteObject(bitmap);
            if (memory) ::DeleteDC(memory);
            if (screen) ::ReleaseDC(nullptr, screen);
            const auto before = ::GetGuiResources(::GetCurrentProcess(), GR_GDIOBJECTS);
            for (int index = 0; result == 0 && index < 100; ++index) {
                HDC dc = ::CreateCompatibleDC(nullptr);
                if (!dc) { result = 4; break; }
                ::DeleteDC(dc);
            }
            const auto after = ::GetGuiResources(::GetCurrentProcess(), GR_GDIOBJECTS);
            if (result == 0 && after > before + 1) result = 5;
        } catch (...) {
            result = 6;
        }
        ::PostQuitMessage(result);
    }

    std::vector<std::wstring> lines_;
    std::vector<mwfl::PrintPage> pages_;
    mwfl::PrintPreviewModel preview_;
    mwfl::PrinterSettings settings_;
    mwfl::Button previous_, next_, zoom_out_, zoom_in_, settings_button_, print_;
    mwfl::Label status_;
};
}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
    g_self_test = std::wstring_view{::GetCommandLineW()}.find(L"--self-test") !=
                  std::wstring_view::npos;
    return mwfl::RunApplication<PrintingWindow>(instance, show_command);
}
