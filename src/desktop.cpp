#include <mwtl/desktop.h>

#include <shobjidl.h>

#include <algorithm>
#include <cstring>
#include <limits>
#include <utility>

namespace mwtl {
namespace {

std::wstring Terminate(std::wstring_view value) {
    return std::wstring(value);
}

bool OpenClipboardWithRetry(HWND owner) noexcept {
    constexpr DWORD retry_delays_ms[]{0, 1, 2, 4, 8, 16};
    for (const DWORD delay : retry_delays_ms) {
        if (delay != 0) ::Sleep(delay);
        if (::OpenClipboard(owner) != FALSE) return true;
        if (::GetLastError() != ERROR_ACCESS_DENIED) break;
    }
    return false;
}

template <typename Interface>
class ComPtr final {
public:
    ~ComPtr() { if (value_ != nullptr) value_->Release(); }
    Interface** Put() noexcept { return &value_; }
    Interface* operator->() const noexcept { return value_; }
    Interface* Get() const noexcept { return value_; }
private:
    Interface* value_ = nullptr;
};

class ComApartment final {
public:
    ComApartment() noexcept : result_(::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)) {}
    ~ComApartment() { if (SUCCEEDED(result_)) ::CoUninitialize(); }
    HRESULT Result() const noexcept { return result_; }
private:
    HRESULT result_;
};

FileDialogResult DialogResult(IFileDialog& dialog) {
    ComPtr<IShellItem> item;
    HRESULT result = dialog.GetResult(item.Put());
    if (FAILED(result)) return {{}, static_cast<DWORD>(result), false};
    PWSTR native_path = nullptr;
    result = item->GetDisplayName(SIGDN_FILESYSPATH, &native_path);
    if (FAILED(result)) return {{}, static_cast<DWORD>(result), false};
    struct FreePath { PWSTR value; ~FreePath() { ::CoTaskMemFree(value); } } free_path{native_path};
    return {std::filesystem::path(native_path), 0, true};
}

void SetInitialPath(IFileDialog& dialog, const std::filesystem::path& path) {
    if (path.empty()) return;
    const auto folder = std::filesystem::is_directory(path) ? path : path.parent_path();
    if (!folder.empty()) {
        ComPtr<IShellItem> item;
        if (SUCCEEDED(::SHCreateItemFromParsingName(
                folder.c_str(), nullptr, IID_PPV_ARGS(item.Put())))) {
            dialog.SetFolder(item.Get());
        }
    }
    if (!std::filesystem::is_directory(path) && !path.filename().empty()) {
        dialog.SetFileName(path.filename().c_str());
    }
}

FileDialogResult ShowFileDialog(const FileDialogOptions& options, bool save) {
    ComApartment apartment;
    if (FAILED(apartment.Result()) && apartment.Result() != RPC_E_CHANGED_MODE) {
        return {{}, static_cast<DWORD>(apartment.Result()), false};
    }
    ComPtr<IFileDialog> dialog;
    const HRESULT created = save
        ? ::CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER,
                             IID_PPV_ARGS(dialog.Put()))
        : ::CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                             IID_PPV_ARGS(dialog.Put()));
    if (FAILED(created)) return {{}, static_cast<DWORD>(created), false};

    FILEOPENDIALOGOPTIONS flags{};
    dialog->GetOptions(&flags);
    flags |= FOS_FORCEFILESYSTEM | FOS_NOCHANGEDIR;
    if (options.path_must_exist) flags |= FOS_PATHMUSTEXIST;
    if (save) flags |= FOS_OVERWRITEPROMPT; else flags |= FOS_FILEMUSTEXIST;
    dialog->SetOptions(flags);
    if (!options.title.empty()) dialog->SetTitle(options.title.c_str());
    if (!options.default_extension.empty()) {
        dialog->SetDefaultExtension(options.default_extension.c_str());
    }
    std::vector<COMDLG_FILTERSPEC> filters;
    filters.reserve(options.filters.size());
    for (const auto& filter : options.filters) {
        filters.push_back({filter.name.c_str(), filter.pattern.c_str()});
    }
    if (!filters.empty()) {
        dialog->SetFileTypes(static_cast<UINT>(filters.size()), filters.data());
    }
    SetInitialPath(*dialog.Get(), options.initial_path);
    const HRESULT shown = dialog->Show(options.owner);
    if (shown == HRESULT_FROM_WIN32(ERROR_CANCELLED)) return {};
    if (FAILED(shown)) return {{}, static_cast<DWORD>(shown), false};
    return DialogResult(*dialog.Get());
}

RECT ClampRectToWorkArea(RECT rect) noexcept {
    const HMONITOR monitor = ::MonitorFromRect(&rect, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    if (monitor == nullptr || ::GetMonitorInfoW(monitor, &info) == FALSE) {
        return rect;
    }
    const LONG width = (std::min)(rect.right - rect.left, info.rcWork.right - info.rcWork.left);
    const LONG height = (std::min)(rect.bottom - rect.top, info.rcWork.bottom - info.rcWork.top);
    rect.left = (std::clamp)(rect.left, info.rcWork.left, info.rcWork.right - width);
    rect.top = (std::clamp)(rect.top, info.rcWork.top, info.rcWork.bottom - height);
    rect.right = rect.left + width;
    rect.bottom = rect.top + height;
    return rect;
}

}  // namespace

Menu::~Menu() noexcept { Reset(); }
Menu::Menu(Menu&& other) noexcept
    : menu_(std::exchange(other.menu_, nullptr)),
      owns_menu_(std::exchange(other.owns_menu_, false)) {}
Menu& Menu::operator=(Menu&& other) noexcept {
    if (this != &other) {
        Reset();
        menu_ = std::exchange(other.menu_, nullptr);
        owns_menu_ = std::exchange(other.owns_menu_, false);
    }
    return *this;
}
bool Menu::Create() noexcept {
    Reset();
    menu_ = ::CreateMenu();
    owns_menu_ = menu_ != nullptr;
    return menu_ != nullptr;
}
bool Menu::CreatePopup() noexcept {
    Reset();
    menu_ = ::CreatePopupMenu();
    owns_menu_ = menu_ != nullptr;
    return menu_ != nullptr;
}
bool Menu::AppendCommand(UINT id, std::wstring_view text, bool enabled) {
    const std::wstring value = Terminate(text);
    return menu_ != nullptr && ::AppendMenuW(menu_, MF_STRING | (enabled ? MF_ENABLED : MF_GRAYED), id, value.c_str()) != FALSE;
}
bool Menu::AppendSeparator() noexcept {
    return menu_ != nullptr && ::AppendMenuW(menu_, MF_SEPARATOR, 0, nullptr) != FALSE;
}
bool Menu::AppendSubmenu(Menu&& submenu, std::wstring_view text, bool enabled) {
    if (menu_ == nullptr || submenu.menu_ == nullptr) return false;
    const std::wstring value = Terminate(text);
    if (::AppendMenuW(menu_, MF_POPUP | (enabled ? MF_ENABLED : MF_GRAYED),
                      reinterpret_cast<UINT_PTR>(submenu.menu_), value.c_str()) == FALSE) return false;
    submenu.menu_ = nullptr;
    submenu.owns_menu_ = false;
    return true;
}
bool Menu::AttachToWindow(HWND window) noexcept {
    if (menu_ == nullptr || window == nullptr) return false;
    const HMENU previous = ::GetMenu(window);
    if (::SetMenu(window, menu_) == FALSE) return false;
    owns_menu_ = false;
    if (previous != nullptr && previous != menu_) ::DestroyMenu(previous);
    return ::DrawMenuBar(window) != FALSE;
}
UINT Menu::Track(HWND owner, POINT point, UINT flags) const noexcept {
    return menu_ == nullptr ? 0 : ::TrackPopupMenu(menu_, flags, point.x, point.y, 0, owner, nullptr);
}
void Menu::Reset() noexcept {
    if (menu_ == nullptr) return;
    if (owns_menu_) ::DestroyMenu(menu_);
    menu_ = nullptr;
    owns_menu_ = false;
}

AcceleratorTable::~AcceleratorTable() noexcept { Reset(); }
AcceleratorTable::AcceleratorTable(AcceleratorTable&& other) noexcept : table_(std::exchange(other.table_, nullptr)) {}
AcceleratorTable& AcceleratorTable::operator=(AcceleratorTable&& other) noexcept {
    if (this != &other) { Reset(); table_ = std::exchange(other.table_, nullptr); }
    return *this;
}
bool AcceleratorTable::Create(std::span<const ACCEL> entries) noexcept {
    Reset();
    if (entries.empty() || entries.size() > static_cast<std::size_t>(INT_MAX)) return false;
    table_ = ::CreateAcceleratorTableW(const_cast<ACCEL*>(entries.data()), static_cast<int>(entries.size()));
    return table_ != nullptr;
}
void AcceleratorTable::Reset() noexcept { if (table_ != nullptr) ::DestroyAcceleratorTable(std::exchange(table_, nullptr)); }

FileDialogResult ShowOpenFileDialog(const FileDialogOptions& options) { return ShowFileDialog(options, false); }
FileDialogResult ShowSaveFileDialog(const FileDialogOptions& options) { return ShowFileDialog(options, true); }
FileDialogResult ShowFolderDialog(const FolderDialogOptions& options) {
    ComApartment apartment;
    if (FAILED(apartment.Result()) && apartment.Result() != RPC_E_CHANGED_MODE) {
        return {{}, static_cast<DWORD>(apartment.Result()), false};
    }
    ComPtr<IFileOpenDialog> dialog;
    const HRESULT created = ::CoCreateInstance(
        CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(dialog.Put()));
    if (FAILED(created)) return {{}, static_cast<DWORD>(created), false};
    FILEOPENDIALOGOPTIONS flags{};
    dialog->GetOptions(&flags);
    dialog->SetOptions(flags | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM |
                       FOS_PATHMUSTEXIST | FOS_NOCHANGEDIR);
    if (!options.title.empty()) dialog->SetTitle(options.title.c_str());
    SetInitialPath(*dialog.Get(), options.initial_path);
    const HRESULT shown = dialog->Show(options.owner);
    if (shown == HRESULT_FROM_WIN32(ERROR_CANCELLED)) return {};
    if (FAILED(shown)) return {{}, static_cast<DWORD>(shown), false};
    return DialogResult(*dialog.Get());
}
bool AcceleratorTable::Create(const CommandSet& commands) {
    std::vector<ACCEL> entries;
    entries.reserve(commands.GetCount());
    for (const Command& command : commands.GetCommands()) {
        if (!command.GetShortcut()) continue;
        if (command.GetId().value <= 0 || command.GetId().value >
                static_cast<int>((std::numeric_limits<WORD>::max)())) {
            return false;
        }
        const CommandShortcut shortcut = *command.GetShortcut();
        entries.push_back(ACCEL{shortcut.modifiers, shortcut.key,
            static_cast<WORD>(command.GetId().value)});
    }
    return Create(std::span<const ACCEL>{entries});
}

bool Menu::AppendCommand(const Command& command) {
    if (!command.IsVisible()) return true;
    if (!AppendCommand(static_cast<UINT>(command.GetId().value),
                       command.GetText(), command.IsEnabled())) {
        return false;
    }
    if (command.IsChecked()) {
        return ::CheckMenuItem(menu_, static_cast<UINT>(command.GetId().value),
            MF_BYCOMMAND | MF_CHECKED) != static_cast<DWORD>(-1);
    }
    return true;
}

bool Menu::UpdateCommand(const Command& command) noexcept {
    if (menu_ == nullptr || command.GetId().value < 0) return false;
    const UINT id = static_cast<UINT>(command.GetId().value);
    if (!command.IsVisible()) {
        return ::DeleteMenu(menu_, id, MF_BYCOMMAND) != FALSE;
    }
    const UINT enabled = command.IsEnabled() ? MF_ENABLED : MF_GRAYED;
    const UINT checked = command.IsChecked() ? MF_CHECKED : MF_UNCHECKED;
    const bool enabled_ok = ::EnableMenuItem(menu_, id, MF_BYCOMMAND | enabled) !=
        -1;
    const bool checked_ok = ::CheckMenuItem(menu_, id, MF_BYCOMMAND | checked) !=
        static_cast<DWORD>(-1);
    MENUITEMINFOW item{};
    item.cbSize = sizeof(item);
    item.fMask = MIIM_STRING;
    const std::wstring text{command.GetText()};
    item.dwTypeData = const_cast<wchar_t*>(text.c_str());
    const bool text_ok = ::SetMenuItemInfoW(menu_, id, FALSE, &item) != FALSE;
    return enabled_ok && checked_ok && text_ok;
}

bool SetClipboardText(HWND owner, std::wstring_view text) noexcept {
    if (!OpenClipboardWithRetry(owner)) return false;
    struct Close { ~Close() { ::CloseClipboard(); } } close;
    if (::EmptyClipboard() == FALSE) return false;
    const SIZE_T bytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL memory = ::GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (memory == nullptr) return false;
    void* target = ::GlobalLock(memory);
    if (target == nullptr) { ::GlobalFree(memory); return false; }
    std::memcpy(target, text.data(), text.size() * sizeof(wchar_t));
    static_cast<wchar_t*>(target)[text.size()] = L'\0';
    ::GlobalUnlock(memory);
    if (::SetClipboardData(CF_UNICODETEXT, memory) == nullptr) { ::GlobalFree(memory); return false; }
    return true;
}

std::wstring GetClipboardText(HWND owner) {
    if (!OpenClipboardWithRetry(owner)) return {};
    struct Close { ~Close() { ::CloseClipboard(); } } close;
    HANDLE memory = ::GetClipboardData(CF_UNICODETEXT);
    if (memory == nullptr) return {};
    const auto* text = static_cast<const wchar_t*>(::GlobalLock(memory));
    if (text == nullptr) return {};
    std::wstring result(text);
    ::GlobalUnlock(memory);
    return result;
}

void EnableFileDrop(HWND window, bool enabled) noexcept { if (window != nullptr) ::DragAcceptFiles(window, enabled ? TRUE : FALSE); }
std::vector<std::wstring> ReadDroppedFiles(HDROP drop) {
    std::vector<std::wstring> files;
    if (drop == nullptr) return files;
    struct Finish { HDROP value; ~Finish() { ::DragFinish(value); } } finish{drop};
    const UINT count = ::DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
    files.reserve(count);
    for (UINT index = 0; index < count; ++index) {
        const UINT length = ::DragQueryFileW(drop, index, nullptr, 0);
        std::wstring path(static_cast<std::size_t>(length) + 1, L'\0');
        const UINT copied = ::DragQueryFileW(drop, index, path.data(), length + 1);
        path.resize(copied);
        files.push_back(std::move(path));
    }
    return files;
}

bool CaptureWindowPlacement(HWND window, SavedWindowPlacement& output) noexcept {
    output.placement.length = sizeof(WINDOWPLACEMENT);
    return window != nullptr && ::GetWindowPlacement(window, &output.placement) != FALSE;
}
bool RestoreWindowPlacement(HWND window, const SavedWindowPlacement& saved, bool clamp) noexcept {
    if (window == nullptr || saved.placement.length != sizeof(WINDOWPLACEMENT)) return false;
    WINDOWPLACEMENT placement = saved.placement;
    if (placement.showCmd == SW_SHOWMINIMIZED || placement.showCmd == SW_MINIMIZE || placement.showCmd == SW_SHOWMINNOACTIVE) placement.showCmd = SW_SHOWNORMAL;
    if (clamp) placement.rcNormalPosition = ClampRectToWorkArea(placement.rcNormalPosition);
    return ::SetWindowPlacement(window, &placement) != FALSE;
}
bool SaveWindowPlacementToRegistry(HKEY root, std::wstring_view subkey, std::wstring_view value_name, const SavedWindowPlacement& placement) {
    const std::wstring key_name = Terminate(subkey), name = Terminate(value_name);
    HKEY key = nullptr;
    if (::RegCreateKeyExW(root, key_name.c_str(), 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS) return false;
    const LONG status = ::RegSetValueExW(key, name.c_str(), 0, REG_BINARY,
        reinterpret_cast<const BYTE*>(&placement), sizeof(placement));
    ::RegCloseKey(key);
    return status == ERROR_SUCCESS;
}
bool LoadWindowPlacementFromRegistry(HKEY root, std::wstring_view subkey, std::wstring_view value_name, SavedWindowPlacement& placement) {
    const std::wstring key_name = Terminate(subkey), name = Terminate(value_name);
    HKEY key = nullptr;
    if (::RegOpenKeyExW(root, key_name.c_str(), 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) return false;
    DWORD type = 0, size = sizeof(placement);
    SavedWindowPlacement loaded{};
    const LONG status = ::RegQueryValueExW(key, name.c_str(), nullptr, &type,
        reinterpret_cast<BYTE*>(&loaded), &size);
    ::RegCloseKey(key);
    if (status != ERROR_SUCCESS || type != REG_BINARY || size != sizeof(loaded) || loaded.placement.length != sizeof(WINDOWPLACEMENT)) return false;
    placement = loaded;
    return true;
}

UiFont::~UiFont() noexcept { Reset(); }
UiFont::UiFont(UiFont&& other) noexcept : font_(std::exchange(other.font_, nullptr)) {}
UiFont& UiFont::operator=(UiFont&& other) noexcept { if (this != &other) { Reset(); font_ = std::exchange(other.font_, nullptr); } return *this; }
bool UiFont::CreateMessageFont(UINT dpi) noexcept {
    Reset();
    NONCLIENTMETRICSW metrics{};
    metrics.cbSize = sizeof(metrics);
    if (::SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0, dpi == 0 ? 96 : dpi) == FALSE) return false;
    font_ = ::CreateFontIndirectW(&metrics.lfMessageFont);
    return font_ != nullptr;
}
void UiFont::Reset() noexcept { if (font_ != nullptr) ::DeleteObject(std::exchange(font_, nullptr)); }
void SetControlFont(HWND control, HFONT font, bool redraw) noexcept { if (control != nullptr) ::SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), redraw ? TRUE : FALSE); }
HWND FocusNextControl(HWND parent, HWND current, bool previous) noexcept {
    if (parent == nullptr) return nullptr;
    HWND next = ::GetNextDlgTabItem(parent, current, previous ? TRUE : FALSE);
    return next != nullptr ? ::SetFocus(next) : nullptr;
}

}  // namespace mwtl
