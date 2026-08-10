#include <mwtl/help.h>
#include <mwtl/shell_integration.h>

#include <array>

class DesktopCommands final {
public:
    mwtl::ShellResult Create(HWND window) noexcept {
        owner_ = window;
        auto result = taskbar_.Create(window);
        if (!result) return result;
        return taskbar_.SetThumbnailButtons(buttons_);
    }

    mwtl::ShellResult ExplorerRestarted() noexcept {
        auto recreated = taskbar_.Recreate();
        if (!recreated) return recreated;
        return taskbar_.SetThumbnailButtons(buttons_);
    }

    mwtl::HelpResult OpenHelp() noexcept {
        return mwtl::LaunchHelp(owner_, {mwtl::HelpTargetKind::https_uri, {},
            L"https://example.com/help", L"#commands"});
    }

    void Close() noexcept { taskbar_.Clear(); taskbar_.Reset(); }

private:
    const std::array<mwtl::TaskbarThumbnailButton, 1> buttons_{
        mwtl::TaskbarThumbnailButton{1, nullptr, L"Help", THBF_ENABLED}};
    HWND owner_ = nullptr;
    mwtl::TaskbarWindowIntegration taskbar_;
};
