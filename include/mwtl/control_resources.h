#pragma once

#include <windows.h>
#include <commctrl.h>

#include <cstdint>
#include <memory>
#include <exception>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace mwtl {

class ImageList final {
public:
    ImageList() noexcept = default;
    ~ImageList() noexcept;
    ImageList(const ImageList&) = delete;
    ImageList& operator=(const ImageList&) = delete;
    ImageList(ImageList&& other) noexcept;
    ImageList& operator=(ImageList&& other) noexcept;
    bool Create(int width, int height, UINT flags = ILC_COLOR32 | ILC_MASK, int initial = 4, int grow = 4) noexcept;
    int AddIcon(HICON icon) noexcept;
    int GetCount() const noexcept;
    HIMAGELIST GetHandle() const noexcept { return handle_; }
    void Reset() noexcept;
private:
    HIMAGELIST handle_ = nullptr;
};

class Tooltip final {
public:
    Tooltip() noexcept = default;
    ~Tooltip() noexcept;
    Tooltip(const Tooltip&) = delete;
    Tooltip& operator=(const Tooltip&) = delete;
    Tooltip(Tooltip&& other) noexcept;
    Tooltip& operator=(Tooltip&& other) noexcept;
    bool Create(HWND owner);
    bool AddTool(HWND tool, std::wstring_view text);
    HWND GetHwnd() const noexcept { return window_; }
    void Destroy() noexcept;
private:
    HWND window_ = nullptr;
    HWND owner_ = nullptr;
    std::vector<std::unique_ptr<std::wstring>> texts_;
};

struct TaskDialogButton {
    int id = 0;
    std::wstring text;
};

enum class TaskDialogEventKind {
    created,
    navigated,
    button_clicked,
    hyperlink_clicked,
    timer,
    destroyed,
    radio_button_clicked,
    verification_clicked,
    help,
    expando_button_clicked,
};

struct TaskDialogEvent {
    TaskDialogEventKind kind = TaskDialogEventKind::created;
    HWND window = nullptr;  // Borrowed and callback-scoped.
    int id = 0;
    bool checked = false;
    std::uint32_t timer_milliseconds = 0;
    std::wstring_view hyperlink;
};

enum class TaskDialogCallbackResult {
    continue_default,
    keep_open,
};

class TaskDialogController final {
public:
    explicit TaskDialogController(HWND window) noexcept : window_(window) {}
    HWND GetHwnd() const noexcept { return window_; }
    bool ClickButton(int id) const noexcept;
    bool ClickRadioButton(int id) const noexcept;
    bool ClickVerification(bool checked, bool focus = false) const noexcept;
    bool EnableButton(int id, bool enabled) const noexcept;
    bool EnableRadioButton(int id, bool enabled) const noexcept;
    bool SetProgressRange(std::uint16_t minimum, std::uint16_t maximum) const noexcept;
    bool SetProgressPosition(int position) const noexcept;
    bool SetProgressState(int state) const noexcept;
    bool SetProgressMarquee(bool enabled, UINT interval_milliseconds = 0) const noexcept;
    bool SetElementText(TASKDIALOG_ELEMENTS element, std::wstring_view text) const noexcept;

private:
    // The controller borrows a native dialog HWND and is valid only during the
    // callback invocation that supplied it. Every operation is UI-thread-only.
    HWND window_ = nullptr;
};

using TaskDialogCallback =
    std::function<TaskDialogCallbackResult(const TaskDialogEvent&, const TaskDialogController&)>;

struct TaskDialogOptions {
    HWND owner = nullptr;  // Borrowed for the duration of the modal call.
    std::wstring title;
    std::wstring main_instruction;
    std::wstring content;
    std::wstring verification_text;
    std::wstring expanded_information;
    std::wstring expanded_control_text;
    std::wstring collapsed_control_text;
    std::wstring footer;
    TASKDIALOG_COMMON_BUTTON_FLAGS common_buttons = TDCBF_OK_BUTTON;
    TASKDIALOG_FLAGS flags = TDF_ALLOW_DIALOG_CANCELLATION;
    std::vector<TaskDialogButton> buttons;
    std::vector<TaskDialogButton> radio_buttons;
    int default_button = 0;
    int default_radio_button = 0;
    bool verification_checked = false;
    UINT width = 0;
    TaskDialogCallback callback;
};

struct TaskDialogResult {
    HRESULT status = E_FAIL;
    int button = 0;
    int radio_button = 0;
    bool verification_checked = false;
    std::exception_ptr callback_exception;
    explicit operator bool() const noexcept { return SUCCEEDED(status); }
};

bool IsTaskDialogAvailable() noexcept;
TaskDialogResult ShowTaskDialog(const TaskDialogOptions& options) noexcept;
TaskDialogResult ShowTaskDialog(HWND owner, std::wstring_view title, std::wstring_view instruction, std::wstring_view content, TASKDIALOG_COMMON_BUTTON_FLAGS buttons = TDCBF_OK_BUTTON) noexcept;

bool InitializeFlatScrollBars(HWND window) noexcept;
bool UninitializeFlatScrollBars(HWND window) noexcept;

}  // namespace mwtl
