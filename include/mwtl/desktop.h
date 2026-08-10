#pragma once

#include <windows.h>
#include <shellapi.h>
#include <shlobj_core.h>

#include <span>
#include <concepts>
#include <filesystem>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

#include <mwtl/command.h>

namespace mwtl {

enum class MenuKind {
    none,
    menu_bar,
    popup,
    attached,
};

enum class PopupMenuStatus {
    selected,
    cancelled,
    failed,
};

struct PopupMenuResult {
    PopupMenuStatus status = PopupMenuStatus::failed;
    UINT command = 0;
    DWORD error = ERROR_SUCCESS;

    explicit operator bool() const noexcept { return status == PopupMenuStatus::selected; }
    bool Cancelled() const noexcept { return status == PopupMenuStatus::cancelled; }
};

class Menu final {
public:
    Menu() noexcept = default;
    ~Menu() noexcept;
    Menu(const Menu&) = delete;
    Menu& operator=(const Menu&) = delete;
    Menu(Menu&& other) noexcept;
    Menu& operator=(Menu&& other) noexcept;

    bool Create() noexcept;
    bool CreatePopup() noexcept;
    bool AppendCommand(UINT id, std::wstring_view text,
                                     bool enabled = true);
    bool AppendCommand(const Command& command);
    bool UpdateCommand(const Command& command) noexcept;
    bool AppendSeparator() noexcept;
    bool AppendSubmenu(Menu&& submenu, std::wstring_view text,
                                     bool enabled = true);
    bool AttachToWindow(HWND window) noexcept;
    PopupMenuResult TrackResult(
        HWND owner, POINT screen_position,
        UINT flags = TPM_RIGHTBUTTON | TPM_RETURNCMD) const noexcept;
    UINT Track(HWND owner, POINT screen_position,
                             UINT flags = TPM_RIGHTBUTTON | TPM_RETURNCMD) const noexcept;
    MenuKind GetKind() const noexcept { return kind_; }
    bool IsOwned() const noexcept { return owns_menu_; }
    // Borrowed escape hatch. Ownership is reported by IsOwned/GetKind.
    HMENU GetHandle() const noexcept { return menu_; }
    void Reset() noexcept;

private:
    HMENU menu_ = nullptr;
    bool owns_menu_ = false;
    MenuKind kind_ = MenuKind::none;
};

class AcceleratorTable final {
public:
    AcceleratorTable() noexcept = default;
    ~AcceleratorTable() noexcept;
    AcceleratorTable(const AcceleratorTable&) = delete;
    AcceleratorTable& operator=(const AcceleratorTable&) = delete;
    AcceleratorTable(AcceleratorTable&& other) noexcept;
    AcceleratorTable& operator=(AcceleratorTable&& other) noexcept;

    bool Create(std::span<const ACCEL> entries) noexcept;
    bool Create(const CommandSet& commands);
    template <std::ranges::input_range Range>
        requires std::convertible_to<std::ranges::range_reference_t<Range>, ACCEL>
    bool Create(Range&& entries) {
        std::vector<ACCEL> values;
        if constexpr (std::ranges::sized_range<Range>) {
            values.reserve(std::ranges::size(entries));
        }
        for (auto&& entry : entries) values.push_back(entry);
        return Create(std::span<const ACCEL>{values});
    }
    HACCEL GetHandle() const noexcept { return table_; }
    void Reset() noexcept;

private:
    HACCEL table_ = nullptr;
};

struct FileDialogOptions {
    HWND owner = nullptr;
    std::wstring title;
    struct Filter {
        std::wstring name;
        std::wstring pattern;
    };
    std::vector<Filter> filters;
    std::wstring default_extension;
    std::filesystem::path initial_path;
    bool path_must_exist = true;
};

struct FileDialogResult {
    std::filesystem::path path;
    DWORD extended_error = 0;
    bool accepted = false;

    bool Cancelled() const noexcept {
        return !accepted && extended_error == 0;
    }
};

FileDialogResult ShowOpenFileDialog(const FileDialogOptions& options);
FileDialogResult ShowSaveFileDialog(const FileDialogOptions& options);

struct FolderDialogOptions {
    HWND owner = nullptr;
    std::wstring title;
    std::filesystem::path initial_path;
};

FileDialogResult ShowFolderDialog(
    const FolderDialogOptions& options = {});

bool SetClipboardText(HWND owner, std::wstring_view text) noexcept;
std::wstring GetClipboardText(HWND owner = nullptr);

void EnableFileDrop(HWND window, bool enabled = true) noexcept;
std::vector<std::wstring> ReadDroppedFiles(HDROP drop);

struct SavedWindowPlacement {
    WINDOWPLACEMENT placement = [] {
        WINDOWPLACEMENT value{};
        value.length = sizeof(value);
        return value;
    }();
};

bool CaptureWindowPlacement(HWND window,
                                          SavedWindowPlacement& output) noexcept;
bool RestoreWindowPlacement(HWND window,
                                          const SavedWindowPlacement& saved,
                                          bool clamp_to_visible_work_area = true) noexcept;
bool SaveWindowPlacementToRegistry(
    HKEY root, std::wstring_view subkey, std::wstring_view value_name,
    const SavedWindowPlacement& placement);
bool LoadWindowPlacementFromRegistry(
    HKEY root, std::wstring_view subkey, std::wstring_view value_name,
    SavedWindowPlacement& placement);

class UiFont final {
public:
    UiFont() noexcept = default;
    ~UiFont() noexcept;
    UiFont(const UiFont&) = delete;
    UiFont& operator=(const UiFont&) = delete;
    UiFont(UiFont&& other) noexcept;
    UiFont& operator=(UiFont&& other) noexcept;

    bool CreateMessageFont(UINT dpi) noexcept;
    HFONT GetHandle() const noexcept { return font_; }
    void Reset() noexcept;

private:
    HFONT font_ = nullptr;
};

void SetControlFont(HWND control, HFONT font, bool redraw = true) noexcept;
HWND FocusNextControl(HWND parent, HWND current,
                                    bool previous = false) noexcept;

}  // namespace mwtl
