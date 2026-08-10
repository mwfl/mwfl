#include <mwfl/mdi.h>

#include <shellapi.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>

namespace {

constexpr UINT CommandNew = 100;
constexpr UINT CommandDirty = 101;
constexpr UINT CommandClose = 102;
constexpr UINT CommandCascade = 110;
constexpr UINT CommandTileHorizontal = 111;
constexpr UINT CommandTileVertical = 112;
constexpr UINT CommandExit = 120;
constexpr UINT RunSelfTest = WM_APP + 80;

HMENU CreateApplicationMenu() {
    HMENU menu = ::CreateMenu();
    HMENU file = ::CreatePopupMenu();
    HMENU window = ::CreatePopupMenu();
    ::AppendMenuW(file, MF_STRING, CommandNew, L"&New\tCtrl+N");
    ::AppendMenuW(file, MF_STRING, CommandDirty, L"Mark &modified");
    ::AppendMenuW(file, MF_STRING, CommandClose, L"&Close\tCtrl+W");
    ::AppendMenuW(file, MF_SEPARATOR, 0, nullptr);
    ::AppendMenuW(file, MF_STRING, CommandExit, L"E&xit");
    ::AppendMenuW(window, MF_STRING, CommandCascade, L"&Cascade");
    ::AppendMenuW(window, MF_STRING, CommandTileHorizontal, L"Tile &horizontally");
    ::AppendMenuW(window, MF_STRING, CommandTileVertical, L"Tile &vertically");
    ::AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(file), L"&File");
    ::AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(window), L"&Window");
    return menu;
}

class MdiApplication final {
public:
    MdiApplication(bool self_test, std::optional<std::filesystem::path> result)
        : self_test_(self_test), result_(std::move(result)) {}

    int Run(HINSTANCE instance, int show) {
        WNDCLASSW window_class{};
        window_class.lpfnWndProc = WindowProcedure;
        window_class.hInstance = instance;
        window_class.hCursor = ::LoadCursorW(nullptr, IDC_ARROW);
        window_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        window_class.lpszClassName = L"mwfl.mdi.reference.frame";
        if (!::RegisterClassW(&window_class)) return Fail(L"frame registration failed");
        menu_ = CreateApplicationMenu();
        frame_ = ::CreateWindowExW(0, window_class.lpszClassName,
            L"mwfl Legacy MDI Workspace", WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, 960, 680, nullptr, menu_, instance, this);
        if (!frame_) return Fail(L"frame creation failed");
        HMENU window_menu = ::GetSubMenu(menu_, 1);
        if (!host_.Create(frame_, model_, window_menu))
            return Fail(L"MDI client creation failed");
        ACCEL accelerators[] = {
            {FCONTROL | FVIRTKEY, 'N', CommandNew},
            {FCONTROL | FVIRTKEY, 'W', CommandClose}};
        accelerators_ = ::CreateAcceleratorTableW(
            accelerators, static_cast<int>(std::size(accelerators)));
        if (!accelerators_) return Fail(L"accelerator creation failed");
        CreateDocument();
        CreateDocument();
        ::ShowWindow(frame_, self_test_ ? SW_HIDE : show);
        ::UpdateWindow(frame_);
        if (self_test_) ::PostMessageW(frame_, RunSelfTest, 0, 0);
        MSG message{};
        while (const BOOL fetched = ::GetMessageW(&message, nullptr, 0, 0)) {
            if (fetched == -1) return Fail(L"message loop failed");
            if (!::TranslateAcceleratorW(frame_, accelerators_, &message) &&
                !host_.Translate(message)) {
                ::TranslateMessage(&message);
                ::DispatchMessageW(&message);
            }
        }
        return static_cast<int>(message.wParam);
    }

private:
    static LRESULT CALLBACK WindowProcedure(HWND window, UINT message,
                                            WPARAM wparam, LPARAM lparam) {
        auto* self = reinterpret_cast<MdiApplication*>(
            ::GetWindowLongPtrW(window, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lparam);
            self = static_cast<MdiApplication*>(create->lpCreateParams);
            self->frame_ = window;
            ::SetWindowLongPtrW(window, GWLP_USERDATA,
                               reinterpret_cast<LONG_PTR>(self));
        }
        return self ? self->HandleMessage(message, wparam, lparam)
                    : ::DefWindowProcW(window, message, wparam, lparam);
    }

    LRESULT HandleMessage(UINT message, WPARAM wparam, LPARAM lparam) {
        switch (message) {
        case WM_SIZE: {
            RECT bounds{0, 0, LOWORD(lparam), HIWORD(lparam)};
            host_.Resize(bounds);
            return 0;
        }
        case WM_COMMAND:
            return HandleCommand(LOWORD(wparam));
        case RunSelfTest:
            RunDeterministicSelfTest();
            return 0;
        case WM_CLOSE:
            if (ConfirmCloseAll()) ::DestroyWindow(frame_);
            return 0;
        case WM_DESTROY:
            host_.Destroy();
            if (accelerators_) {
                ::DestroyAcceleratorTable(accelerators_);
                accelerators_ = nullptr;
            }
            ::PostQuitMessage(self_test_failed_ ? 1 : 0);
            return 0;
        }
        return host_.GetClient() ? host_.FrameDefault(message, wparam, lparam)
                                 : ::DefWindowProcW(frame_, message, wparam, lparam);
    }

    LRESULT HandleCommand(UINT command) {
        switch (command) {
        case CommandNew: CreateDocument(); break;
        case CommandDirty:
            if (const auto active = model_.GetActiveId()) {
                model_.SetDirty(*active, true);
                RefreshTitle(*active);
            }
            break;
        case CommandClose: CloseActive(false); break;
        case CommandCascade: host_.Arrange(mwfl::MdiArrange::cascade); break;
        case CommandTileHorizontal: host_.Arrange(mwfl::MdiArrange::tile_horizontal); break;
        case CommandTileVertical: host_.Arrange(mwfl::MdiArrange::tile_vertical); break;
        case CommandExit: ::SendMessageW(frame_, WM_CLOSE, 0, 0); break;
        default: return host_.FrameDefault(WM_COMMAND, command, 0);
        }
        return 0;
    }

    void CreateDocument() {
        const mwfl::MdiChildId id{next_id_++};
        const std::wstring title = L"Document " + std::to_wstring(id.value);
        if (!model_.Add({id, title})) { FailSelfTest(L"model add failed"); return; }
        auto created = host_.CreateChild(id, {WS_VISIBLE | WS_OVERLAPPEDWINDOW,
            [this, id](HWND window, UINT message, WPARAM, LPARAM)
                -> std::optional<LRESULT> {
                if (message == WM_PAINT) {
                    PAINTSTRUCT paint{};
                    HDC dc = ::BeginPaint(window, &paint);
                    const auto* document = model_.Find(id);
                    const std::wstring text = document && document->dirty
                        ? L"This document has unsaved changes." : L"This document is clean.";
                    RECT bounds{};
                    ::GetClientRect(window, &bounds);
                    ::DrawTextW(dc, text.c_str(), -1, &bounds,
                                DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                    ::EndPaint(window, &paint);
                    return 0;
                }
                return std::nullopt;
            }});
        if (!created) { model_.Remove(id, true); FailSelfTest(L"child creation failed"); }
    }

    void RefreshTitle(mwfl::MdiChildId id) {
        const auto* document = model_.Find(id);
        HWND child = host_.GetChild(id);
        if (!document || !child) return;
        std::wstring title = document->title;
        if (document->dirty) title += L" *";
        ::SetWindowTextW(child, title.c_str());
        ::InvalidateRect(child, nullptr, TRUE);
    }

    bool CloseActive(bool discard) {
        const auto active = model_.GetActiveId();
        if (!active) return true;
        const auto* document = model_.Find(*active);
        if (!document) return false;
        if (document->dirty && !discard) {
            if (self_test_) return false;
            if (::MessageBoxW(frame_, L"Discard unsaved changes?", L"mwfl MDI",
                    MB_OKCANCEL | MB_ICONWARNING) != IDOK) return false;
        }
        return static_cast<bool>(host_.Close(*active));
    }

    bool ConfirmCloseAll() {
        bool dirty = false;
        for (const auto& child : model_.GetChildren()) dirty |= child.dirty;
        if (dirty && !self_test_ && ::MessageBoxW(frame_,
                L"Discard all unsaved changes?", L"mwfl MDI",
                MB_OKCANCEL | MB_ICONWARNING) != IDOK) return false;
        while (const auto active = model_.GetActiveId())
            if (!host_.Close(*active, true)) return false;
        return true;
    }

    void RunDeterministicSelfTest() {
        if (model_.GetChildren().size() != 2) return FailSelfTest(L"initial children");
        ::SendMessageW(frame_, WM_COMMAND, CommandDirty, 0);
        const auto dirty = model_.GetActiveId();
        if (!dirty || !model_.Find(*dirty)->dirty) return FailSelfTest(L"dirty routing");
        const auto count = model_.GetChildren().size();
        if (CloseActive(false) || model_.GetChildren().size() != count)
            return FailSelfTest(L"cancel was not atomic");
        if (!host_.Activate(model_.GetChildren().front().id))
            return FailSelfTest(L"activation failed");
        ::SendMessageW(frame_, WM_COMMAND, CommandCascade, 0);
        ::SendMessageW(frame_, WM_COMMAND, CommandTileHorizontal, 0);
        ::SendMessageW(frame_, WM_COMMAND, CommandTileVertical, 0);
        ::SendMessageW(frame_, WM_COMMAND, CommandNew, 0);
        if (model_.GetChildren().size() != 3 || !CloseActive(true))
            return FailSelfTest(L"create/close failed");
        if (!ConfirmCloseAll() || !model_.GetChildren().empty())
            return FailSelfTest(L"coordinated shutdown failed");
        WriteResult(L"ok");
        ::DestroyWindow(frame_);
    }

    int Fail(std::wstring_view text) { WriteResult(text); return 1; }
    void FailSelfTest(std::wstring_view text) {
        self_test_failed_ = true;
        WriteResult(text);
        if (frame_ && ::IsWindow(frame_)) ::DestroyWindow(frame_);
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
    bool self_test_failed_ = false;
    std::optional<std::filesystem::path> result_;
    HWND frame_ = nullptr;
    HMENU menu_ = nullptr;
    HACCEL accelerators_ = nullptr;
    mwfl::MdiWorkspaceModel model_;
    mwfl::MdiHost host_;
    std::uint64_t next_id_ = 1;
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
    MdiApplication application{self_test, std::move(result)};
    return application.Run(instance, show);
}
