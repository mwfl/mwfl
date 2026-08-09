#include <mwtl/control_resources.h>

#include <wil/resource.h>

#include <cstring>
#include <string>
#include <unordered_set>
#include <utility>

namespace mwtl {

namespace {

using TaskDialogIndirectFunction = HRESULT(WINAPI*)(const TASKDIALOGCONFIG*, int*, int*, BOOL*);

TaskDialogIndirectFunction LoadTaskDialogIndirect(HMODULE module) noexcept {
    const FARPROC address = ::GetProcAddress(module, "TaskDialogIndirect");
    static_assert(sizeof(address) == sizeof(TaskDialogIndirectFunction));
    TaskDialogIndirectFunction function = nullptr;
    std::memcpy(&function, &address, sizeof(function));
    return function;
}

struct TaskDialogCallbackContext {
    const TaskDialogCallback* callback = nullptr;
    TaskDialogResult* result = nullptr;
};

HRESULT CALLBACK DispatchTaskDialogEvent(HWND window, UINT notification, WPARAM wparam,
                                         LPARAM lparam, LONG_PTR callback_data) noexcept {
    auto* context = reinterpret_cast<TaskDialogCallbackContext*>(callback_data);
    if (context == nullptr || context->callback == nullptr || !*context->callback) return S_OK;
    TaskDialogEvent event{};
    event.window = window;
    switch (notification) {
        case TDN_CREATED: event.kind = TaskDialogEventKind::created; break;
        case TDN_NAVIGATED: event.kind = TaskDialogEventKind::navigated; break;
        case TDN_BUTTON_CLICKED:
            event.kind = TaskDialogEventKind::button_clicked;
            event.id = static_cast<int>(wparam);
            break;
        case TDN_HYPERLINK_CLICKED:
            event.kind = TaskDialogEventKind::hyperlink_clicked;
            if (lparam != 0) event.hyperlink = reinterpret_cast<const wchar_t*>(lparam);
            break;
        case TDN_TIMER:
            event.kind = TaskDialogEventKind::timer;
            event.timer_milliseconds = static_cast<std::uint32_t>(wparam);
            break;
        case TDN_DESTROYED: event.kind = TaskDialogEventKind::destroyed; break;
        case TDN_RADIO_BUTTON_CLICKED:
            event.kind = TaskDialogEventKind::radio_button_clicked;
            event.id = static_cast<int>(wparam);
            break;
        case TDN_VERIFICATION_CLICKED:
            event.kind = TaskDialogEventKind::verification_clicked;
            event.checked = wparam != 0;
            break;
        case TDN_HELP: event.kind = TaskDialogEventKind::help; break;
        case TDN_EXPANDO_BUTTON_CLICKED:
            event.kind = TaskDialogEventKind::expando_button_clicked;
            event.checked = wparam != 0;
            break;
        default: return S_OK;
    }
    try {
        const TaskDialogCallbackResult response =
            (*context->callback)(event, TaskDialogController{window});
        return notification == TDN_BUTTON_CLICKED &&
                       response == TaskDialogCallbackResult::keep_open
                   ? S_FALSE
                   : S_OK;
    } catch (...) {
        if (context->result != nullptr) {
            context->result->callback_exception = std::current_exception();
            context->result->status = E_UNEXPECTED;
        }
        if (notification != TDN_DESTROYED) {
            ::PostMessageW(window, TDM_CLICK_BUTTON, IDCANCEL, 0);
        }
        return S_OK;
    }
}

bool IsUsableTaskDialogWindow(HWND window) noexcept {
    return window != nullptr && ::IsWindow(window) != FALSE;
}

const wchar_t* OptionalText(const std::wstring& text) noexcept {
    return text.empty() ? nullptr : text.c_str();
}

bool ContainsCommonButton(TASKDIALOG_COMMON_BUTTON_FLAGS buttons, int id) noexcept {
    switch (id) {
        case IDOK: return (buttons & TDCBF_OK_BUTTON) != 0;
        case IDYES: return (buttons & TDCBF_YES_BUTTON) != 0;
        case IDNO: return (buttons & TDCBF_NO_BUTTON) != 0;
        case IDCANCEL: return (buttons & TDCBF_CANCEL_BUTTON) != 0;
        case IDRETRY: return (buttons & TDCBF_RETRY_BUTTON) != 0;
        case IDCLOSE: return (buttons & TDCBF_CLOSE_BUTTON) != 0;
        default: return false;
    }
}

}  // namespace

ImageList::~ImageList() noexcept { Reset(); }
ImageList::ImageList(ImageList&& other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {}
ImageList& ImageList::operator=(ImageList&& other) noexcept { if (this != &other) { Reset(); handle_ = std::exchange(other.handle_, nullptr); } return *this; }
bool ImageList::Create(int width, int height, UINT flags, int initial, int grow) noexcept { Reset(); handle_ = ImageList_Create(width, height, flags, initial, grow); return handle_ != nullptr; }
int ImageList::AddIcon(HICON icon) noexcept { return handle_ != nullptr && icon != nullptr ? ImageList_ReplaceIcon(handle_, -1, icon) : -1; }
int ImageList::GetCount() const noexcept { return handle_ != nullptr ? ImageList_GetImageCount(handle_) : 0; }
void ImageList::Reset() noexcept { if (handle_ != nullptr) ImageList_Destroy(handle_); handle_ = nullptr; }

Tooltip::~Tooltip() noexcept { Destroy(); }
Tooltip::Tooltip(Tooltip&& other) noexcept : window_(std::exchange(other.window_, nullptr)), owner_(std::exchange(other.owner_, nullptr)), texts_(std::move(other.texts_)) {}
Tooltip& Tooltip::operator=(Tooltip&& other) noexcept { if (this != &other) { Destroy(); window_ = std::exchange(other.window_, nullptr); owner_ = std::exchange(other.owner_, nullptr); texts_ = std::move(other.texts_); } return *this; }
bool Tooltip::Create(HWND owner) { Destroy(); if (owner == nullptr) { ::SetLastError(ERROR_INVALID_WINDOW_HANDLE); return false; } INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_WIN95_CLASSES}; if (::InitCommonControlsEx(&controls) == FALSE) return false; window_ = ::CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr, WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, owner, nullptr, reinterpret_cast<HINSTANCE>(::GetWindowLongPtrW(owner, GWLP_HINSTANCE)), nullptr); if (window_ == nullptr) return false; owner_ = owner; ::SetWindowPos(window_, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE); return true; }
bool Tooltip::AddTool(HWND tool, std::wstring_view text) { if (window_ == nullptr || tool == nullptr || ::GetParent(tool) != owner_) return false; texts_.push_back(std::make_unique<std::wstring>(text)); TOOLINFOW info{}; info.cbSize = sizeof(info); info.uFlags = TTF_IDISHWND | TTF_SUBCLASS; info.hwnd = owner_; info.uId = reinterpret_cast<UINT_PTR>(tool); info.lpszText = texts_.back()->data(); if (::SendMessageW(window_, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&info)) == FALSE) { texts_.pop_back(); return false; } return true; }
void Tooltip::Destroy() noexcept { if (window_ != nullptr && ::IsWindow(window_) != FALSE) ::DestroyWindow(window_); window_ = nullptr; owner_ = nullptr; texts_.clear(); }

bool TaskDialogController::ClickButton(int id) const noexcept {
    if (!IsUsableTaskDialogWindow(window_)) return false;
    ::SendMessageW(window_, TDM_CLICK_BUTTON, static_cast<WPARAM>(id), 0);
    return true;
}

bool TaskDialogController::ClickRadioButton(int id) const noexcept {
    if (!IsUsableTaskDialogWindow(window_)) return false;
    ::SendMessageW(window_, TDM_CLICK_RADIO_BUTTON, static_cast<WPARAM>(id), 0);
    return true;
}

bool TaskDialogController::ClickVerification(bool checked, bool focus) const noexcept {
    if (!IsUsableTaskDialogWindow(window_)) return false;
    ::SendMessageW(window_, TDM_CLICK_VERIFICATION, checked ? TRUE : FALSE, focus ? TRUE : FALSE);
    return true;
}

bool TaskDialogController::EnableButton(int id, bool enabled) const noexcept {
    if (!IsUsableTaskDialogWindow(window_)) return false;
    ::SendMessageW(window_, TDM_ENABLE_BUTTON, static_cast<WPARAM>(id), enabled ? TRUE : FALSE);
    return true;
}

bool TaskDialogController::EnableRadioButton(int id, bool enabled) const noexcept {
    if (!IsUsableTaskDialogWindow(window_)) return false;
    ::SendMessageW(window_, TDM_ENABLE_RADIO_BUTTON, static_cast<WPARAM>(id),
                   enabled ? TRUE : FALSE);
    return true;
}

bool TaskDialogController::SetProgressRange(std::uint16_t minimum,
                                            std::uint16_t maximum) const noexcept {
    if (!IsUsableTaskDialogWindow(window_) || minimum > maximum) return false;
    ::SendMessageW(window_, TDM_SET_PROGRESS_BAR_RANGE, 0, MAKELPARAM(minimum, maximum));
    return true;
}

bool TaskDialogController::SetProgressPosition(int position) const noexcept {
    if (!IsUsableTaskDialogWindow(window_)) return false;
    ::SendMessageW(window_, TDM_SET_PROGRESS_BAR_POS, static_cast<WPARAM>(position), 0);
    return true;
}

bool TaskDialogController::SetProgressState(int state) const noexcept {
    if (!IsUsableTaskDialogWindow(window_)) return false;
    ::SendMessageW(window_, TDM_SET_PROGRESS_BAR_STATE, static_cast<WPARAM>(state), 0);
    return true;
}

bool TaskDialogController::SetProgressMarquee(bool enabled, UINT interval_milliseconds) const
    noexcept {
    if (!IsUsableTaskDialogWindow(window_)) return false;
    ::SendMessageW(window_, TDM_SET_PROGRESS_BAR_MARQUEE, enabled ? TRUE : FALSE,
                   interval_milliseconds);
    return true;
}

bool TaskDialogController::SetElementText(TASKDIALOG_ELEMENTS element,
                                          std::wstring_view text) const noexcept {
    if (!IsUsableTaskDialogWindow(window_)) return false;
    try {
        const std::wstring owned{text};
        ::SendMessageW(window_, TDM_SET_ELEMENT_TEXT, static_cast<WPARAM>(element),
                       reinterpret_cast<LPARAM>(owned.c_str()));
        return true;
    } catch (...) {
        ::SetLastError(ERROR_OUTOFMEMORY);
        return false;
    }
}

bool IsTaskDialogAvailable() noexcept {
    const HMODULE module = ::LoadLibraryW(L"comctl32.dll");
    if (module == nullptr) return false;
    const bool available = LoadTaskDialogIndirect(module) != nullptr;
    ::FreeLibrary(module);
    return available;
}

TaskDialogResult ShowTaskDialog(const TaskDialogOptions& options) noexcept {
    TaskDialogResult result;
    try {
        const HMODULE common_controls = ::LoadLibraryW(L"comctl32.dll");
        if (common_controls == nullptr) {
            result.status = HRESULT_FROM_WIN32(::GetLastError());
            return result;
        }
        const auto release_common_controls = wil::scope_exit(
            [common_controls]() noexcept { ::FreeLibrary(common_controls); });

        const TaskDialogIndirectFunction task_dialog = LoadTaskDialogIndirect(common_controls);
        if (task_dialog == nullptr) {
            result.status = HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);
            return result;
        }

        std::unordered_set<int> button_ids;
        std::vector<TASKDIALOG_BUTTON> native_buttons;
        native_buttons.reserve(options.buttons.size());
        for (const TaskDialogButton& button : options.buttons) {
            if (button.id <= 0 || button.text.empty() ||
                ContainsCommonButton(options.common_buttons, button.id) ||
                !button_ids.insert(button.id).second) {
                result.status = E_INVALIDARG;
                return result;
            }
            native_buttons.push_back({button.id, button.text.c_str()});
        }
        std::unordered_set<int> radio_ids;
        std::vector<TASKDIALOG_BUTTON> native_radios;
        native_radios.reserve(options.radio_buttons.size());
        for (const TaskDialogButton& button : options.radio_buttons) {
            if (button.id <= 0 || button.text.empty() || !radio_ids.insert(button.id).second) {
                result.status = E_INVALIDARG;
                return result;
            }
            native_radios.push_back({button.id, button.text.c_str()});
        }
        if (options.default_radio_button != 0 &&
            !radio_ids.contains(options.default_radio_button)) {
            result.status = E_INVALIDARG;
            return result;
        }
        if (options.default_button != 0 && !button_ids.contains(options.default_button) &&
            !ContainsCommonButton(options.common_buttons, options.default_button)) {
            result.status = E_INVALIDARG;
            return result;
        }
        TaskDialogCallbackContext callback_context{&options.callback, &result};
        TASKDIALOGCONFIG config{};
        config.cbSize = sizeof(config);
        config.hwndParent = options.owner;
        config.dwFlags = options.flags;
        if (options.verification_checked) config.dwFlags |= TDF_VERIFICATION_FLAG_CHECKED;
        config.dwCommonButtons = options.common_buttons;
        config.pszWindowTitle = OptionalText(options.title);
        config.pszMainInstruction = OptionalText(options.main_instruction);
        config.pszContent = OptionalText(options.content);
        config.cButtons = static_cast<UINT>(native_buttons.size());
        config.pButtons = native_buttons.data();
        config.nDefaultButton = options.default_button;
        config.cRadioButtons = static_cast<UINT>(native_radios.size());
        config.pRadioButtons = native_radios.data();
        config.nDefaultRadioButton = options.default_radio_button;
        config.pszVerificationText = OptionalText(options.verification_text);
        config.pszExpandedInformation = OptionalText(options.expanded_information);
        config.pszExpandedControlText = OptionalText(options.expanded_control_text);
        config.pszCollapsedControlText = OptionalText(options.collapsed_control_text);
        config.pszFooter = OptionalText(options.footer);
        config.pfCallback = DispatchTaskDialogEvent;
        config.lpCallbackData = reinterpret_cast<LONG_PTR>(&callback_context);
        config.cxWidth = options.width;
        BOOL checked = options.verification_checked ? TRUE : FALSE;
        result.status = task_dialog(&config, &result.button, &result.radio_button, &checked);
        result.verification_checked = checked != FALSE;
        if (result.callback_exception) result.status = E_UNEXPECTED;
    } catch (...) {
        result.status = E_OUTOFMEMORY;
    }
    return result;
}

TaskDialogResult ShowTaskDialog(HWND owner, std::wstring_view title,
                                std::wstring_view instruction, std::wstring_view content,
                                TASKDIALOG_COMMON_BUTTON_FLAGS buttons) noexcept {
    try {
        TaskDialogOptions options{};
        options.owner = owner;
        options.title = std::wstring(title);
        options.main_instruction = std::wstring(instruction);
        options.content = std::wstring(content);
        options.common_buttons = buttons;
        return ShowTaskDialog(options);
    } catch (...) {
        TaskDialogResult failed{};
        failed.status = E_OUTOFMEMORY;
        return failed;
    }
}

bool InitializeFlatScrollBars(HWND window) noexcept { return window != nullptr && ::InitializeFlatSB(window) != FALSE; }
bool UninitializeFlatScrollBars(HWND window) noexcept { return window != nullptr && ::UninitializeFlatSB(window) != FALSE; }

}  // namespace mwtl
