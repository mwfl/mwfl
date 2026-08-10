#include <mwfl/ribbon.h>
#include <mwfl/settings_store.h>

#include <shellapi.h>

#include <array>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr UINT RunSelfTest = WM_APP + 90;
constexpr mwfl::RibbonCommandId ModeRibbon{1010};
constexpr mwfl::RibbonCommandId ContextRibbon{1020};
constexpr mwfl::RibbonCommandId RecentRibbon{1030};
constexpr mwfl::ControlId ModeCommand{2010};
constexpr mwfl::ControlId ContextCommand{2020};
constexpr mwfl::ControlId RecentCommand{2030};

class RibbonApplication final {
public:
    RibbonApplication(bool self_test, std::optional<std::filesystem::path> result)
        : self_test_(self_test), result_(std::move(result)),
          settings_(HKEY_CURRENT_USER,
              self_test ? L"Software\\mwfl\\Tests\\Ribbon-" +
                              std::to_wstring(::GetCurrentProcessId())
                        : L"Software\\mwfl\\Examples\\RibbonWorkspace", 1) {}

    int Run(HINSTANCE instance, int show) {
        const HRESULT initialized = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        if (FAILED(initialized)) return Fail(L"STA initialization failed");
        com_initialized_ = true;
        WNDCLASSW type{};
        type.lpfnWndProc = Procedure;
        type.hInstance = instance;
        type.hCursor = ::LoadCursorW(nullptr, IDC_ARROW);
        type.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        type.lpszClassName = L"mwfl.ribbon.reference";
        if (!::RegisterClassW(&type)) return Fail(L"frame registration failed");
        frame_ = ::CreateWindowExW(0, type.lpszClassName, L"mwfl Ribbon Workspace",
            WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 900, 620,
            nullptr, nullptr, instance, this);
        if (!frame_) return Fail(L"frame creation failed");
        content_ = ::CreateWindowExW(0, L"STATIC", L"Ribbon command state",
            WS_CHILD | WS_VISIBLE | SS_CENTER, 0, 0, 0, 0, frame_, nullptr, instance, nullptr);
        if (!content_) return Fail(L"content creation failed");
        BuildCommands();
        const auto created = ribbon_.Create(frame_, model_, commands_);
        if (created) ribbon_available_ = true;
        else if (created.status != mwfl::RibbonHostStatus::unavailable)
            return Fail(L"Ribbon creation failed");
        if (ribbon_available_ &&
            !ribbon_.Load(instance, L"MWFL_RIBBON_REFERENCE_RIBBON"))
            return Fail(L"Ribbon resource load failed");
        LoadSettings();
        const std::array recent{
            mwfl::RibbonRecentItem{L"welcome", L"Welcome", L"C:\\welcome.txt"},
            mwfl::RibbonRecentItem{L"notes", L"Notes", L"C:\\notes.txt"}};
        if (!model_.SetRecentItems(recent)) return Fail(L"recent items failed");
        UpdatePresentation();
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

    ~RibbonApplication() {
        ribbon_.Destroy();
        if (com_initialized_) ::CoUninitialize();
    }

private:
    static LRESULT CALLBACK Procedure(HWND window, UINT message, WPARAM wparam,
                                      LPARAM lparam) {
        auto* self = reinterpret_cast<RibbonApplication*>(
            ::GetWindowLongPtrW(window, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lparam);
            self = static_cast<RibbonApplication*>(create->lpCreateParams);
            self->frame_ = window;
            ::SetWindowLongPtrW(window, GWLP_USERDATA,
                               reinterpret_cast<LONG_PTR>(self));
        }
        return self ? self->Handle(message, wparam, lparam)
                    : ::DefWindowProcW(window, message, wparam, lparam);
    }

    LRESULT Handle(UINT message, WPARAM wparam, LPARAM lparam) {
        switch (message) {
        case WM_SIZE: Layout(LOWORD(lparam), HIWORD(lparam)); return 0;
        case WM_DPICHANGED: {
            const auto* bounds = reinterpret_cast<const RECT*>(lparam);
            ::SetWindowPos(frame_, nullptr, bounds->left, bounds->top,
                bounds->right - bounds->left, bounds->bottom - bounds->top,
                SWP_NOACTIVATE | SWP_NOZORDER);
            if (ribbon_available_) native_ok_ = native_ok_ && ribbon_.InvalidateAll();
            return 0;
        }
        case WM_THEMECHANGED:
            if (ribbon_available_) native_ok_ = native_ok_ && ribbon_.InvalidateAll();
            ::InvalidateRect(content_, nullptr, TRUE);
            return 0;
        case RunSelfTest: RunSelfTestNow(); return 0;
        case WM_CLOSE: SaveSettings(); ::DestroyWindow(frame_); return 0;
        case WM_DESTROY: ribbon_.Destroy(); ::PostQuitMessage(failed_ ? 1 : 0); return 0;
        }
        return ::DefWindowProcW(frame_, message, wparam, lparam);
    }

    void BuildCommands() {
        commands_.Add(mwfl::Command(ModeCommand, L"Switch mode", [this] {
            mode_ = mode_ == 1 ? 2u : 1u;
            model_.SetActiveModes(mode_);
            if (ribbon_available_) native_ok_ = native_ok_ && ribbon_.SetModes(mode_);
            UpdatePresentation();
        }));
        commands_.Add(mwfl::Command(ContextCommand, L"Toggle context", [this] {
            context_visible_ = !context_visible_;
            model_.SetContextVisible({7}, context_visible_);
            if (ribbon_available_) native_ok_ = native_ok_ && ribbon_.InvalidateAll();
            UpdatePresentation();
        }));
        commands_.Add(mwfl::Command(RecentCommand, L"Add recent item", [this] {
            auto items = std::vector<mwfl::RibbonRecentItem>(
                model_.GetRecentItems().begin(), model_.GetRecentItems().end());
            items.push_back({L"item-" + std::to_wstring(items.size()),
                L"Recent item", L"C:\\recent.txt"});
            model_.SetRecentItems(items);
            UpdatePresentation();
        }));
        model_.Add({ModeRibbon, ModeCommand, 3});
        model_.Add({ContextRibbon, ContextCommand, 1});
        model_.Add({RecentRibbon, RecentCommand, 2, {7}});
    }

    void Layout(int width, int height) {
        const int top = static_cast<int>(ribbon_.GetHeight());
        ::MoveWindow(content_, 0, top, width, (std::max)(0, height - top), TRUE);
    }

    void UpdatePresentation() {
        std::wstring text = L"Mode " + std::to_wstring(mode_) +
            L" | context " + (context_visible_ ? L"visible" : L"hidden") +
            L" | recent " + std::to_wstring(model_.GetRecentItems().size());
        ::SetWindowTextW(content_, text.c_str());
        if (ribbon_available_) native_ok_ = native_ok_ && ribbon_.InvalidateAll();
    }

    void LoadSettings() {
        const std::array schema{
            mwfl::SettingDefinition{L"Mode", mwfl::SettingType::dword, 4, true},
            mwfl::SettingDefinition{L"Context", mwfl::SettingType::dword, 4, true}};
        const auto loaded = settings_.Load(schema);
        if (loaded) {
            if (const auto* value = mwfl::FindSetting(loaded.values, L"Mode"))
                mode_ = std::get<std::uint32_t>(value->data) == 2 ? 2u : 1u;
            if (const auto* value = mwfl::FindSetting(loaded.values, L"Context"))
                context_visible_ = std::get<std::uint32_t>(value->data) != 0;
        }
        model_.SetActiveModes(mode_);
        model_.SetContextVisible({7}, context_visible_);
        if (ribbon_available_) native_ok_ = native_ok_ && ribbon_.SetModes(mode_);
    }

    bool SaveSettings() {
        const std::array values{
            mwfl::SettingValue{L"Mode", mode_},
            mwfl::SettingValue{L"Context", context_visible_ ? 1u : 0u}};
        return static_cast<bool>(settings_.Save(values));
    }

    void RunSelfTestNow() {
        if ((ribbon_available_ && !ribbon_.IsCreated()) || !native_ok_ ||
            model_.GetRecentItems().size() != 2)
            return FailSelfTest(L"initial Ribbon state");
        if (!model_.Execute(ModeRibbon, commands_) || mode_ != 2)
            return FailSelfTest(L"mode command routing");
        if (!model_.Execute(ContextRibbon, commands_) || !context_visible_)
            return FailSelfTest(L"context command routing");
        if (!model_.Execute(RecentRibbon, commands_) || model_.GetRecentItems().size() != 3)
            return FailSelfTest(L"recent command routing");
        auto projection = model_.Project(commands_);
        if (!projection || !projection.commands[2].visible)
            return FailSelfTest(L"mode/context projection");
        ::SendMessageW(frame_, WM_THEMECHANGED, 0, 0);
        if (!native_ok_) return FailSelfTest(L"native Ribbon update");
        if (!SaveSettings()) return FailSelfTest(L"settings save");
        const std::array schema{
            mwfl::SettingDefinition{L"Mode", mwfl::SettingType::dword, 4, true},
            mwfl::SettingDefinition{L"Context", mwfl::SettingType::dword, 4, true}};
        if (!settings_.Load(schema)) return FailSelfTest(L"settings reload");
        const std::array<std::wstring_view, 2> names{L"Mode", L"Context"};
        settings_.RemoveOwned(names);
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
    bool com_initialized_ = false;
    bool context_visible_ = false;
    bool ribbon_available_ = false;
    bool native_ok_ = true;
    std::uint32_t mode_ = 1;
    std::optional<std::filesystem::path> result_;
    HWND frame_ = nullptr;
    HWND content_ = nullptr;
    mwfl::RibbonCommandModel model_;
    mwfl::CommandSet commands_;
    mwfl::RibbonFrameworkHost ribbon_;
    mwfl::VersionedSettingsStore settings_;
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
    RibbonApplication application{self_test, std::move(result)};
    return application.Run(instance, show);
}
