#include <mwfl/scintilla.h>
#include <mwfl/appearance.h>

#include <Scintilla.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <new>
#include <utility>
#include <vector>

namespace mwfl {
namespace detail {

struct ScintillaRuntimeState final {
    explicit ScintillaRuntimeState(HMODULE input) noexcept : module(input) {}
    ~ScintillaRuntimeState() noexcept {
        if (module) ::FreeLibrary(module);
    }
    HMODULE module = nullptr;
};

}  // namespace detail
namespace {

DWORD ValidError(DWORD fallback) noexcept {
    const DWORD error = ::GetLastError();
    return error == ERROR_SUCCESS ? fallback : error;
}

}  // namespace

bool ScintillaRuntime::Load(const std::filesystem::path& dll_path) noexcept {
    Reset();
    if (dll_path.empty()) {
        status_ = ScintillaRuntimeStatus::missing;
        error_ = ERROR_FILE_NOT_FOUND;
        return false;
    }
    std::filesystem::path absolute_path;
    try {
        absolute_path = std::filesystem::absolute(dll_path);
    } catch (...) {
        status_ = ScintillaRuntimeStatus::failed;
        error_ = ERROR_INVALID_NAME;
        return false;
    }
    ::SetLastError(ERROR_SUCCESS);
    const HMODULE module = ::LoadLibraryExW(absolute_path.c_str(), nullptr,
                                             LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR |
                                                 LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if (!module) {
        error_ = ValidError(ERROR_MOD_NOT_FOUND);
        status_ = error_ == ERROR_BAD_EXE_FORMAT ? ScintillaRuntimeStatus::wrong_architecture
                                                 : (error_ == ERROR_MOD_NOT_FOUND ||
                                                            error_ == ERROR_FILE_NOT_FOUND
                                                        ? ScintillaRuntimeStatus::missing
                                                        : ScintillaRuntimeStatus::failed);
        return false;
    }
    try {
        state_ = std::make_shared<detail::ScintillaRuntimeState>(module);
        loaded_path_ = std::move(absolute_path);
    } catch (...) {
        ::FreeLibrary(module);
        error_ = ERROR_OUTOFMEMORY;
        status_ = ScintillaRuntimeStatus::failed;
        return false;
    }
    status_ = ScintillaRuntimeStatus::available;
    error_ = ERROR_SUCCESS;
    return true;
}

bool ScintillaRuntime::LoadAdjacent() noexcept {
    std::wstring path(32768, L'\0');
    const DWORD length = ::GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (length == 0 || length >= path.size()) {
        Reset();
        status_ = ScintillaRuntimeStatus::failed;
        error_ = ValidError(ERROR_INSUFFICIENT_BUFFER);
        return false;
    }
    path.resize(length);
    return Load(std::filesystem::path(path).parent_path() / L"Scintilla.dll");
}

void ScintillaRuntime::Reset() noexcept {
    state_.reset();
    loaded_path_.clear();
    status_ = ScintillaRuntimeStatus::unloaded;
    error_ = ERROR_SUCCESS;
}

bool ScintillaRuntime::IsAvailable() const noexcept {
    return status_ == ScintillaRuntimeStatus::available && state_ && state_->module;
}

std::optional<std::string> ToUtf8(std::wstring_view text) noexcept {
    if (text.empty()) return std::string{};
    if (text.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)())) return std::nullopt;
    const int size = ::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(),
                                           static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) return std::nullopt;
    try {
        std::string result(static_cast<std::size_t>(size), '\0');
        if (::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(),
                                  static_cast<int>(text.size()), result.data(), size, nullptr,
                                  nullptr) != size)
            return std::nullopt;
        return result;
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<std::wstring> FromUtf8(std::string_view text) noexcept {
    if (text.empty()) return std::wstring{};
    if (text.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)())) return std::nullopt;
    const int size = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                                           static_cast<int>(text.size()), nullptr, 0);
    if (size <= 0) return std::nullopt;
    try {
        std::wstring result(static_cast<std::size_t>(size), L'\0');
        if (::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                                  static_cast<int>(text.size()), result.data(), size) != size)
            return std::nullopt;
        return result;
    } catch (...) {
        return std::nullopt;
    }
}

ScintillaEditor::~ScintillaEditor() noexcept {
    Destroy();
    runtime_.reset();
}

bool ScintillaEditor::Create(HWND parent, ControlId id, RectDip bounds,
                             const ScintillaRuntime& runtime, ScintillaEditorOptions options) {
    if (!runtime.IsAvailable()) {
        ::SetLastError(ERROR_MOD_NOT_FOUND);
        return false;
    }
    Destroy();
    runtime_.reset();
    runtime_ = runtime.state_;
    if (!CreateNative(L"Scintilla", parent, id, L"", bounds, options.style,
                      options.extended_style)) {
        runtime_.reset();
        return false;
    }
    Send(SCI_SETCODEPAGE, SC_CP_UTF8);
    Send(SCI_SETEOLMODE, SC_EOL_LF);
    if (!SetAccessibleName(GetHwnd(), L"Code editor")) {
        Destroy();
        runtime_.reset();
        return false;
    }
    return true;
}

LRESULT ScintillaEditor::Send(UINT message, WPARAM wparam, LPARAM lparam) const noexcept {
    const HWND window = GetHwnd();
    return window ? ::SendMessageW(window, message, wparam, lparam) : 0;
}

bool ScintillaEditor::SetText(std::wstring_view text) noexcept {
    const auto utf8 = ToUtf8(text);
    if (!utf8 || !GetHwnd()) return false;
    Send(SCI_SETTEXT, 0, reinterpret_cast<LPARAM>(utf8->c_str()));
    return true;
}

std::optional<std::wstring> ScintillaEditor::GetText() const noexcept {
    const ScintillaPosition length = GetLength();
    if (length < 0 || static_cast<std::uint64_t>(length) >=
                          static_cast<std::uint64_t>((std::numeric_limits<std::size_t>::max)()))
        return std::nullopt;
    try {
        std::string utf8(static_cast<std::size_t>(length) + 1, '\0');
        Send(SCI_GETTEXT, utf8.size(), reinterpret_cast<LPARAM>(utf8.data()));
        utf8.resize(static_cast<std::size_t>(length));
        return FromUtf8(utf8);
    } catch (...) {
        return std::nullopt;
    }
}

ScintillaPosition ScintillaEditor::GetLength() const noexcept { return Send(SCI_GETLENGTH); }
bool ScintillaEditor::SetReadOnly(bool value) noexcept {
    if (!GetHwnd()) return false;
    Send(SCI_SETREADONLY, value ? 1 : 0);
    return IsReadOnly() == value;
}
bool ScintillaEditor::IsReadOnly() const noexcept { return Send(SCI_GETREADONLY) != 0; }
bool ScintillaEditor::IsModified() const noexcept { return Send(SCI_GETMODIFY) != 0; }
void ScintillaEditor::SetSavePoint() noexcept { Send(SCI_SETSAVEPOINT); }
bool ScintillaEditor::CanUndo() const noexcept { return Send(SCI_CANUNDO) != 0; }
bool ScintillaEditor::CanRedo() const noexcept { return Send(SCI_CANREDO) != 0; }
void ScintillaEditor::Undo() noexcept { Send(SCI_UNDO); }
void ScintillaEditor::Redo() noexcept { Send(SCI_REDO); }

ScintillaTextRange ScintillaEditor::GetSelection() const noexcept {
    return {Send(SCI_GETSELECTIONSTART), Send(SCI_GETSELECTIONEND)};
}

bool ScintillaEditor::SetSelection(ScintillaTextRange range) noexcept {
    if (!GetHwnd() || !range || range.end > GetLength()) return false;
    Send(SCI_SETSEL, static_cast<WPARAM>(range.start), static_cast<LPARAM>(range.end));
    return true;
}

std::optional<ScintillaTextRange> ScintillaEditor::Find(std::wstring_view text,
                                                        ScintillaPosition start,
                                                        ScintillaPosition end,
                                                        ScintillaSearchFlags flags) noexcept {
    const auto utf8 = ToUtf8(text);
    const ScintillaPosition length = GetLength();
    if (!utf8 || utf8->empty() || start < 0 || start > length) return std::nullopt;
    if (end < 0) end = length;
    if (end < start || end > length) return std::nullopt;
    Send(SCI_SETSEARCHFLAGS, static_cast<WPARAM>(static_cast<std::uint32_t>(flags)));
    Send(SCI_SETTARGETSTART, static_cast<WPARAM>(start));
    Send(SCI_SETTARGETEND, static_cast<WPARAM>(end));
    const ScintillaPosition found = Send(SCI_SEARCHINTARGET, utf8->size(),
                                          reinterpret_cast<LPARAM>(utf8->data()));
    if (found < 0) return std::nullopt;
    return ScintillaTextRange{Send(SCI_GETTARGETSTART), Send(SCI_GETTARGETEND)};
}

bool ScintillaEditor::ReplaceTarget(std::wstring_view replacement) noexcept {
    const auto utf8 = ToUtf8(replacement);
    if (!utf8 || !GetHwnd()) return false;
    return Send(SCI_REPLACETARGET, utf8->size(), reinterpret_cast<LPARAM>(utf8->data())) >= 0;
}

bool ScintillaEditor::ConfigureCodeEditing(const ScintillaCodeOptions& options) noexcept {
    if (!GetHwnd() || options.font_size_points <= 0 || options.tab_width <= 0) return false;
    const auto font = ToUtf8(options.font);
    if (!font || font->empty()) return false;
    Send(SCI_STYLESETFONT, STYLE_DEFAULT, reinterpret_cast<LPARAM>(font->c_str()));
    Send(SCI_STYLESETSIZEFRACTIONAL, STYLE_DEFAULT,
         static_cast<LPARAM>(std::lround(options.font_size_points * 100.0f)));
    Send(SCI_STYLECLEARALL);
    Send(SCI_SETTABWIDTH, static_cast<WPARAM>(options.tab_width));
    Send(SCI_SETUSETABS, options.use_tabs ? 1 : 0);
    Send(SCI_SETWRAPMODE, options.word_wrap ? SC_WRAP_WORD : SC_WRAP_NONE);
    Send(SCI_SETMARGINTYPEN, 0, SC_MARGIN_NUMBER);
    if (!options.show_line_numbers) {
        Send(SCI_SETMARGINWIDTHN, 0, 0);
        return true;
    }
    return UpdateLineNumberMargin();
}

bool ScintillaEditor::UpdateLineNumberMargin() noexcept {
    if (!GetHwnd()) return false;
    ScintillaPosition lines = Send(SCI_GETLINECOUNT);
    int digits = 3;
    while (lines >= 1000) {
        ++digits;
        lines /= 10;
    }
    std::string sample(static_cast<std::size_t>(digits), '9');
    sample.push_back('_');
    const LRESULT width = Send(SCI_TEXTWIDTH, STYLE_LINENUMBER,
                               reinterpret_cast<LPARAM>(sample.c_str()));
    if (width <= 0) return false;
    Send(SCI_SETMARGINWIDTHN, 0, width);
    return true;
}

void ScintillaEditor::SelectAll() noexcept { Send(SCI_SELECTALL); }
void ScintillaEditor::Cut() noexcept { Send(SCI_CUT); }
void ScintillaEditor::Copy() noexcept { Send(SCI_COPY); }
void ScintillaEditor::Paste() noexcept { Send(SCI_PASTE); }
void ScintillaEditor::DeleteSelection() noexcept { Send(SCI_CLEAR); }
void ScintillaEditor::SetZoom(int zoom) noexcept { Send(SCI_SETZOOM, static_cast<WPARAM>(zoom)); }
int ScintillaEditor::GetZoom() const noexcept { return static_cast<int>(Send(SCI_GETZOOM)); }

std::optional<ScintillaNotification> ScintillaEditor::DecodeNotification(
    const NMHDR& header) const noexcept {
    if (header.hwndFrom != GetHwnd()) return std::nullopt;
    const auto& notification = reinterpret_cast<const SCNotification&>(header);
    ScintillaNotification decoded;
    decoded.position = notification.position;
    decoded.length = notification.length;
    decoded.line = notification.line;
    decoded.lines_added = notification.linesAdded;
    decoded.modification_flags = notification.modificationType;
    decoded.character = notification.ch;
    decoded.update_flags = notification.updated;
    switch (header.code) {
        case SCN_MODIFIED: decoded.kind = ScintillaNotificationKind::modified; break;
        case SCN_SAVEPOINTREACHED: decoded.kind = ScintillaNotificationKind::save_point_reached; break;
        case SCN_SAVEPOINTLEFT: decoded.kind = ScintillaNotificationKind::save_point_left; break;
        case SCN_CHARADDED: decoded.kind = ScintillaNotificationKind::character_added; break;
        case SCN_UPDATEUI: decoded.kind = ScintillaNotificationKind::update_ui; break;
        default: decoded.kind = ScintillaNotificationKind::other; break;
    }
    return decoded;
}

}  // namespace mwfl
