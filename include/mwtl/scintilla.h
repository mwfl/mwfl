#pragma once

#include <windows.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include <mwtl/controls.h>

namespace mwtl {

inline constexpr std::string_view kScintillaVersion = "5.6.5";
inline constexpr std::string_view kScintillaSourceSha256 =
    "345140a60bf4ceea3340942e8205e6a8fbda8db13eb48f827126b9be15bd3da1";
inline constexpr std::string_view kScintillaRuntimeSha256 =
    "9b5a7af4beb2d61ba6a5f62fa678ff68c96d175d19664fc1a801f444b357303b";

enum class ScintillaRuntimeStatus { unloaded, available, missing, wrong_architecture, failed };

namespace detail {
struct ScintillaRuntimeState;
}

// Loads the pinned Scintilla DLL. Runtime state is shared with every created
// editor, so destroying this wrapper before an editor does not unload its code.
// LoadAdjacent searches only beside the process executable. An explicit path is
// useful for custom deployment; GetLoadedPath reports the actual identity.
class ScintillaRuntime final {
public:
    ScintillaRuntime() noexcept = default;
    ~ScintillaRuntime() noexcept = default;
    ScintillaRuntime(const ScintillaRuntime&) = delete;
    ScintillaRuntime& operator=(const ScintillaRuntime&) = delete;
    ScintillaRuntime(ScintillaRuntime&&) noexcept = default;
    ScintillaRuntime& operator=(ScintillaRuntime&&) noexcept = default;

    bool Load(const std::filesystem::path& dll_path) noexcept;
    bool LoadAdjacent() noexcept;
    void Reset() noexcept;
    bool IsAvailable() const noexcept;
    ScintillaRuntimeStatus GetStatus() const noexcept { return status_; }
    DWORD GetError() const noexcept { return error_; }
    const std::filesystem::path& GetLoadedPath() const noexcept { return loaded_path_; }

private:
    friend class ScintillaEditor;
    std::shared_ptr<detail::ScintillaRuntimeState> state_;
    std::filesystem::path loaded_path_;
    ScintillaRuntimeStatus status_ = ScintillaRuntimeStatus::unloaded;
    DWORD error_ = ERROR_SUCCESS;
};

std::optional<std::string> ToUtf8(std::wstring_view text) noexcept;
std::optional<std::wstring> FromUtf8(std::string_view text) noexcept;

using ScintillaPosition = std::intptr_t;  // UTF-8 byte position, not UTF-16 index.

struct ScintillaTextRange {
    ScintillaPosition start = -1;
    ScintillaPosition end = -1;
    explicit operator bool() const noexcept { return start >= 0 && end >= start; }
};

enum class ScintillaSearchFlags : std::uint32_t {
    none = 0,
    match_case = 1,
    whole_word = 2,
    word_start = 4,
    regular_expression = 0x00200000,
};

constexpr ScintillaSearchFlags operator|(ScintillaSearchFlags left,
                                          ScintillaSearchFlags right) noexcept {
    return static_cast<ScintillaSearchFlags>(static_cast<std::uint32_t>(left) |
                                              static_cast<std::uint32_t>(right));
}

enum class ScintillaNotificationKind {
    modified,
    save_point_reached,
    save_point_left,
    character_added,
    update_ui,
    other,
};

struct ScintillaNotification {
    ScintillaNotificationKind kind = ScintillaNotificationKind::other;
    ScintillaPosition position = -1;
    ScintillaPosition length = 0;
    ScintillaPosition line = 0;
    ScintillaPosition lines_added = 0;
    int modification_flags = 0;
    int character = 0;
    int update_flags = 0;
};

struct ScintillaEditorOptions {
    DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPCHILDREN;
    DWORD extended_style = WS_EX_CLIENTEDGE;
};

// Owns the Scintilla HWND and shares its loaded module. The control owns its
// default Scintilla document; raw document/loader handles returned through Send
// remain Scintilla-owned unless the corresponding native message says otherwise.
// UTF-16 public text is converted strictly to UTF-8. All positions and raw Send
// calls use Scintilla UTF-8 byte offsets and stay on the creating UI thread.
class ScintillaEditor final : public NativeControl {
public:
    ScintillaEditor() noexcept = default;
    ~ScintillaEditor() noexcept;
    ScintillaEditor(const ScintillaEditor&) = delete;
    ScintillaEditor& operator=(const ScintillaEditor&) = delete;
    ScintillaEditor(ScintillaEditor&&) = delete;
    ScintillaEditor& operator=(ScintillaEditor&&) = delete;

    bool Create(HWND parent, ControlId id, RectDip bounds, const ScintillaRuntime& runtime,
                ScintillaEditorOptions options = {});
    template <WindowLike Parent>
    bool Create(const Parent& parent, ControlId id, RectDip bounds,
                const ScintillaRuntime& runtime, ScintillaEditorOptions options = {}) {
        return Create(parent.GetHwnd(), id, bounds, runtime, options);
    }

    bool SetText(std::wstring_view text) noexcept;
    std::optional<std::wstring> GetText() const noexcept;
    ScintillaPosition GetLength() const noexcept;
    bool SetReadOnly(bool read_only) noexcept;
    bool IsReadOnly() const noexcept;
    bool IsModified() const noexcept;
    void SetSavePoint() noexcept;
    bool CanUndo() const noexcept;
    bool CanRedo() const noexcept;
    void Undo() noexcept;
    void Redo() noexcept;
    ScintillaTextRange GetSelection() const noexcept;
    bool SetSelection(ScintillaTextRange range) noexcept;
    std::optional<ScintillaTextRange> Find(std::wstring_view text,
                                            ScintillaPosition start = 0,
                                            ScintillaPosition end = -1,
                                            ScintillaSearchFlags flags = {}) noexcept;
    bool ReplaceTarget(std::wstring_view replacement) noexcept;
    std::optional<ScintillaNotification> DecodeNotification(const NMHDR& header) const noexcept;

    // Native escape hatch. Message/parameters/results follow pinned Scintilla
    // 5.6.5 exactly; pointer lifetimes remain the caller's responsibility.
    LRESULT Send(UINT message, WPARAM wparam = 0, LPARAM lparam = 0) const noexcept;

private:
    std::shared_ptr<detail::ScintillaRuntimeState> runtime_;
};

}  // namespace mwtl
