#include <mwfl/mwfl.h>

#include <array>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

bool accelerator_seen = false;

class AcceleratorWindow final : public mwfl::Window<AcceleratorWindow> {
public:
    void BuildUI() {
        command_set_.Add(mwfl::Command({701}, L"Verify\tF6", [this] {
            accelerator_seen = true;
            ::KillTimer(GetHwnd(), 1);
            ::KillTimer(GetHwnd(), 2);
            static_cast<void>(Close());
        }).SetShortcut({FVIRTKEY, VK_F6}));
        command_set_.Add(mwfl::Command({702}, L"Mutable"));
        mwfl::Menu commands;
        if (!menu_.Create() || !commands.CreatePopup() ||
            !commands.AppendCommand(*command_set_.Find({701})) ||
            !commands.AppendCommand(*command_set_.Find({702})) ||
            !menu_.AppendSubmenu(std::move(commands), L"Test") ||
            !menu_.AttachToWindow(GetHwnd())) {
            throw std::runtime_error("menu fixture failed");
        }
        auto* mutable_command = command_set_.Find({702});
        mutable_command->SetText(L"Updated").SetEnabled(false).SetChecked(true);
        if (!menu_.UpdateCommand(*mutable_command)) {
            throw std::runtime_error("attached menu update failed");
        }
        const HMENU popup = ::GetSubMenu(menu_.GetHandle(), 0);
        wchar_t text[32]{};
        const UINT state = ::GetMenuState(popup, 702, MF_BYCOMMAND);
        if (::GetMenuStringW(popup, 702, text, 32, MF_BYCOMMAND) == 0 ||
            std::wstring_view{text} != L"Updated" ||
            (state & (MF_DISABLED | MF_GRAYED)) == 0 ||
            (state & MF_CHECKED) == 0) {
            throw std::runtime_error("attached menu state did not propagate");
        }
        mutable_command->SetVisible(false);
        if (!menu_.UpdateCommand(*mutable_command) ||
            ::GetMenuState(popup, 702, MF_BYCOMMAND) != static_cast<UINT>(-1)) {
            throw std::runtime_error("hidden menu command was not removed");
        }
        if (!accelerators_.Create(command_set_)) {
            throw std::runtime_error("accelerator fixture failed");
        }
        SetAccelerators(accelerators_.GetHandle());
        ::SetTimer(GetHwnd(), 1, 25, nullptr);
        ::SetTimer(GetHwnd(), 2, 2000, nullptr);
    }

    mwfl::EventResult OnTimer(mwfl::TimerId id) {
        if (id.value == 1) {
            ::SetFocus(GetHwnd());
            ::PostMessageW(GetHwnd(), WM_KEYDOWN, VK_F6, 0);
        } else {
            ::KillTimer(GetHwnd(), id.value);
            (void)Close();
        }
        return mwfl::EventResult::Handled();
    }

    mwfl::EventResult OnCommand(const mwfl::CommandEvent& event) {
        return command_set_.Dispatch(event);
    }

private:
    mwfl::AcceleratorTable accelerators_;
    mwfl::CommandSet command_set_;
    mwfl::Menu menu_;
};

bool VerifyClipboardRoundTrip() {
    const bool had_text = ::IsClipboardFormatAvailable(CF_UNICODETEXT) != FALSE;
    const std::wstring original_text = had_text ? mwfl::GetClipboardText() : L"";

    const bool wrote = mwfl::SetClipboardText(nullptr, L"mwfl clipboard round trip");
    const bool matched = wrote &&
        mwfl::GetClipboardText() == L"mwfl clipboard round trip";
    if (!matched) {
        std::fprintf(stderr, "clipboard round trip failed: wrote=%d error=%lu\n",
                     wrote ? 1 : 0, ::GetLastError());
    }

    bool restored = had_text && mwfl::SetClipboardText(nullptr, original_text);
    if (!had_text && ::OpenClipboard(nullptr) != FALSE) {
        restored = ::EmptyClipboard() != FALSE;
        ::CloseClipboard();
    }
    // Restoration is best-effort: another desktop process can legitimately take
    // clipboard ownership between the verification and cleanup operations.
    (void)restored;
    return matched;
}

bool VerifyDroppedFiles() {
    constexpr wchar_t paths[] = L"C:\\first.txt\0D:\\folder\\second.txt\0\0";
    constexpr SIZE_T path_bytes = sizeof(paths);
    HGLOBAL memory = ::GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT,
                                    sizeof(DROPFILES) + path_bytes);
    if (memory == nullptr) return false;
    auto* drop = static_cast<DROPFILES*>(::GlobalLock(memory));
    if (drop == nullptr) { ::GlobalFree(memory); return false; }
    drop->pFiles = sizeof(DROPFILES);
    drop->fWide = TRUE;
    std::memcpy(reinterpret_cast<BYTE*>(drop) + drop->pFiles, paths, path_bytes);
    ::GlobalUnlock(memory);

    const auto files = mwfl::ReadDroppedFiles(reinterpret_cast<HDROP>(memory));
    return files.size() == 2 && files[0] == L"C:\\first.txt" &&
           files[1] == L"D:\\folder\\second.txt";
}

}  // namespace

int main(int argc, char** argv) {
    const bool clipboard_only = argc == 2 && std::string_view(argv[1]) == "--clipboard-only";
    const bool skip_clipboard = argc == 2 && std::string_view(argv[1]) == "--skip-clipboard";
    if (clipboard_only) return VerifyClipboardRoundTrip() ? EXIT_SUCCESS : 6;

    mwfl::UiFont font;
    if (!font.CreateMessageFont(144) || font.GetHandle() == nullptr) return 1;

    constexpr wchar_t key[] = L"Software\\mwfl\\Tests\\DesktopTest";
    constexpr wchar_t value[] = L"Placement";
    mwfl::SavedWindowPlacement saved{};
    saved.placement.flags = WPF_ASYNCWINDOWPLACEMENT;
    saved.placement.showCmd = SW_SHOWNORMAL;
    saved.placement.rcNormalPosition = {10, 20, 410, 320};
    if (!mwfl::SaveWindowPlacementToRegistry(HKEY_CURRENT_USER, key, value, saved)) return 2;
    mwfl::SavedWindowPlacement loaded{};
    const bool loaded_ok = mwfl::LoadWindowPlacementFromRegistry(
        HKEY_CURRENT_USER, key, value, loaded);
    ::RegDeleteTreeW(HKEY_CURRENT_USER, key);
    if (!loaded_ok || loaded.placement.rcNormalPosition.left != 10 ||
        loaded.placement.rcNormalPosition.bottom != 320) return 3;

    if (!skip_clipboard && !VerifyClipboardRoundTrip()) return 6;
    if (!VerifyDroppedFiles()) return 7;

    const int accelerator_result = mwfl::Application(::GetModuleHandleW(nullptr))
        .Run<AcceleratorWindow>(SW_SHOWNOACTIVATE);
    if (accelerator_result != 0 || !accelerator_seen) return 8;

    return EXIT_SUCCESS;
}
