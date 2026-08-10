#include <mwfl/help.h>
#include <mwfl/shell_integration.h>

#include <array>

class DesktopCommands final {
public:
    mwfl::ShellResult Create(HWND window) noexcept {
        owner_ = window;
        auto result = taskbar_.Create(window);
        if (!result) return result;
        return taskbar_.SetThumbnailButtons(buttons_);
    }

    mwfl::ShellResult ExplorerRestarted() noexcept {
        auto recreated = taskbar_.Recreate();
        if (!recreated) return recreated;
        return taskbar_.SetThumbnailButtons(buttons_);
    }

    mwfl::HelpResult OpenHelp() noexcept {
        return mwfl::LaunchHelp(owner_, {mwfl::HelpTargetKind::https_uri, {},
            L"https://example.com/help", L"#commands"});
    }

    void Close() noexcept { taskbar_.Clear(); taskbar_.Reset(); }

private:
    const std::array<mwfl::TaskbarThumbnailButton, 1> buttons_{
        mwfl::TaskbarThumbnailButton{1, nullptr, L"Help", THBF_ENABLED}};
    HWND owner_ = nullptr;
    mwfl::TaskbarWindowIntegration taskbar_;
};
