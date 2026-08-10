#include <mwtl/graphics.h>

#include <shellapi.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>

namespace {

constexpr UINT ExportPng = 100;
constexpr UINT SaveEmf = 101;
constexpr UINT RunSelfTest = WM_APP + 100;

class GraphicsApplication final {
public:
    GraphicsApplication(bool self_test, std::optional<std::filesystem::path> result)
        : self_test_(self_test), result_(std::move(result)) {}

    int Run(HINSTANCE instance, int show) {
        WNDCLASSW type{};
        type.lpfnWndProc = Procedure;
        type.hInstance = instance;
        type.hCursor = ::LoadCursorW(nullptr, IDC_ARROW);
        type.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        type.lpszClassName = L"mwtl.graphics.interop.reference";
        if (!::RegisterClassW(&type)) return Fail(L"registration failed");
        HMENU menu = ::CreateMenu();
        ::AppendMenuW(menu, MF_STRING, ExportPng, L"Export &PNG");
        ::AppendMenuW(menu, MF_STRING, SaveEmf, L"Save &EMF");
        frame_ = ::CreateWindowExW(0, type.lpszClassName,
            L"mwtl EMF and GDI+ Interop", WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, 760, 560, nullptr, menu, instance, this);
        if (!frame_) return Fail(L"window creation failed");
        auto recorded = mwtl::RecordEnhancedMetafile(nullptr, L"mwtl graphics", [](HDC dc) {
            HPEN pen = ::CreatePen(PS_SOLID, 5, RGB(30, 90, 180));
            HBRUSH brush = ::CreateSolidBrush(RGB(220, 235, 255));
            HGDIOBJ old_pen = ::SelectObject(dc, pen);
            HGDIOBJ old_brush = ::SelectObject(dc, brush);
            ::RoundRect(dc, 20, 20, 520, 320, 32, 32);
            ::MoveToEx(dc, 40, 280, nullptr);
            ::LineTo(dc, 500, 60);
            ::SelectObject(dc, old_brush);
            ::SelectObject(dc, old_pen);
            ::DeleteObject(brush);
            ::DeleteObject(pen);
        });
        if (!recorded) return Fail(L"EMF recording failed");
        metafile_ = std::move(recorded.metafile);
        ::ShowWindow(frame_, self_test_ ? SW_HIDE : show);
        ::UpdateWindow(frame_);
        if (self_test_) ::PostMessageW(frame_, RunSelfTest, 0, 0);
        MSG message{};
        while (const BOOL fetched = ::GetMessageW(&message, nullptr, 0, 0)) {
            if (fetched == -1) return Fail(L"message loop failed");
            ::TranslateMessage(&message);
            ::DispatchMessageW(&message);
        }
        return static_cast<int>(message.wParam);
    }

private:
    static LRESULT CALLBACK Procedure(HWND window, UINT message, WPARAM wparam,
                                      LPARAM lparam) {
        auto* self = reinterpret_cast<GraphicsApplication*>(
            ::GetWindowLongPtrW(window, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lparam);
            self = static_cast<GraphicsApplication*>(create->lpCreateParams);
            self->frame_ = window;
            ::SetWindowLongPtrW(window, GWLP_USERDATA,
                               reinterpret_cast<LONG_PTR>(self));
        }
        return self ? self->Handle(message, wparam, lparam)
                    : ::DefWindowProcW(window, message, wparam, lparam);
    }

    LRESULT Handle(UINT message, WPARAM wparam, LPARAM lparam) {
        switch (message) {
        case WM_PAINT: {
            PAINTSTRUCT paint{};
            HDC dc = ::BeginPaint(frame_, &paint);
            RECT bounds{};
            ::GetClientRect(frame_, &bounds);
            ::InflateRect(&bounds, -30, -30);
            mwtl::PlayEnhancedMetafile(dc, metafile_, bounds);
            ::EndPaint(frame_, &paint);
            return 0;
        }
        case WM_COMMAND:
            if (LOWORD(wparam) == ExportPng) Export(DefaultPath(L"png"));
            else if (LOWORD(wparam) == SaveEmf)
                mwtl::SaveEnhancedMetafile(metafile_, DefaultPath(L"emf"));
            return 0;
        case RunSelfTest: SelfTest(); return 0;
        case WM_DESTROY: ::PostQuitMessage(failed_ ? 1 : 0); return 0;
        }
        return ::DefWindowProcW(frame_, message, wparam, lparam);
    }

    std::filesystem::path DefaultPath(std::wstring_view extension) const {
        return std::filesystem::temp_directory_path() /
            (L"mwtl-graphics-reference-" + std::to_wstring(::GetCurrentProcessId()) +
             L"." + std::wstring(extension));
    }

    mwtl::GraphicsResult Export(const std::filesystem::path& path) {
        return mwtl::ExportGdiPlusPng(path, 640, 400,
            [](Gdiplus::Graphics& graphics) {
                graphics.Clear(Gdiplus::Color(255, 248, 250, 252));
                Gdiplus::SolidBrush brush(Gdiplus::Color(255, 30, 90, 180));
                graphics.FillEllipse(&brush, 70, 50, 500, 280);
                Gdiplus::Pen pen(Gdiplus::Color(255, 255, 255, 255), 6.0f);
                graphics.DrawLine(&pen, 120, 300, 520, 90);
            });
    }

    void SelfTest() {
        const auto emf = DefaultPath(L"emf");
        const auto png = DefaultPath(L"png");
        std::error_code ignored;
        std::filesystem::remove(emf, ignored);
        std::filesystem::remove(png, ignored);
        if (!mwtl::SaveEnhancedMetafile(metafile_, emf))
            return FailSelfTest(L"EMF save failed");
        auto loaded = mwtl::LoadEnhancedMetafile(emf);
        if (!loaded) return FailSelfTest(L"EMF load failed");
        if (!Export(png) || !Export(png)) return FailSelfTest(L"PNG export failed");
        std::ifstream stream(png, std::ios::binary);
        std::array<unsigned char, 8> signature{};
        stream.read(reinterpret_cast<char*>(signature.data()), signature.size());
        constexpr std::array<unsigned char, 8> expected{137, 80, 78, 71, 13, 10, 26, 10};
        if (signature != expected) return FailSelfTest(L"PNG signature failed");
        stream.close();
        std::filesystem::remove(emf, ignored);
        std::filesystem::remove(png, ignored);
        WriteResult(L"ok");
        ::DestroyWindow(frame_);
    }

    int Fail(std::wstring_view text) { WriteResult(text); return 1; }
    void FailSelfTest(std::wstring_view text) {
        failed_ = true;
        WriteResult(text);
        ::DestroyWindow(frame_);
    }
    void WriteResult(std::wstring_view text) {
        if (!result_) return;
        std::ofstream stream(*result_, std::ios::binary | std::ios::trunc);
        const int size = ::WideCharToMultiByte(CP_UTF8, 0, text.data(),
            static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
        std::string encoded(static_cast<std::size_t>(size), '\0');
        if (size > 0)
            ::WideCharToMultiByte(CP_UTF8, 0, text.data(),
                static_cast<int>(text.size()), encoded.data(), size, nullptr, nullptr);
        stream << encoded;
    }

    bool self_test_ = false;
    bool failed_ = false;
    std::optional<std::filesystem::path> result_;
    HWND frame_ = nullptr;
    mwtl::EnhancedMetafile metafile_;
};

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    int count = 0;
    wchar_t** arguments = ::CommandLineToArgvW(::GetCommandLineW(), &count);
    bool self_test = false;
    std::optional<std::filesystem::path> result;
    for (int index = 1; arguments && index < count; ++index) {
        const std::wstring_view argument{arguments[index]};
        if (argument == L"--self-test") self_test = true;
        else if (argument == L"--self-test-result" && index + 1 < count)
            result = arguments[++index];
    }
    if (arguments) ::LocalFree(arguments);
    GraphicsApplication application{self_test, std::move(result)};
    return application.Run(instance, show);
}
