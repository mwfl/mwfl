#include <mwfl/control_resources.h>

#include <wil/resource.h>

#include <climits>
#include <cstring>
#include <string>
#include <unordered_set>
#include <utility>

namespace mwfl {

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
bool ImageList::Create(int width, int height, UINT flags, int initial, int grow) noexcept {
    Reset();
    if (width <= 0 || height <= 0 || initial < 0 || grow < 0) {
        ::SetLastError(ERROR_INVALID_PARAMETER);
        return false;
    }
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_WIN95_CLASSES};
    if (::InitCommonControlsEx(&controls) == FALSE) return false;
    handle_ = ImageList_Create(width, height, flags, initial, grow);
    if (handle_ == nullptr && ::GetLastError() == ERROR_SUCCESS)
        ::SetLastError(ERROR_FUNCTION_FAILED);
    return handle_ != nullptr;
}
int ImageList::AddIcon(HICON icon) noexcept {
    if (handle_ == nullptr || icon == nullptr) {
        ::SetLastError(handle_ == nullptr ? ERROR_INVALID_STATE : ERROR_INVALID_PARAMETER);
        return -1;
    }
    ::SetLastError(ERROR_SUCCESS);
    const int index = ImageList_ReplaceIcon(handle_, -1, icon);
    if (index < 0 && ::GetLastError() == ERROR_SUCCESS) ::SetLastError(ERROR_FUNCTION_FAILED);
    return index;
}
bool ImageList::ReplaceIcon(int index, HICON icon) noexcept {
    if (handle_ == nullptr || icon == nullptr || index < 0 || index >= GetCount()) {
        ::SetLastError(ERROR_INVALID_PARAMETER);
        return false;
    }
    ::SetLastError(ERROR_SUCCESS);
    const bool replaced = ImageList_ReplaceIcon(handle_, index, icon) == index;
    if (!replaced && ::GetLastError() == ERROR_SUCCESS) ::SetLastError(ERROR_FUNCTION_FAILED);
    return replaced;
}
bool ImageList::Remove(int index) noexcept {
    if (handle_ == nullptr || index < 0 || index >= GetCount()) {
        ::SetLastError(ERROR_INVALID_PARAMETER);
        return false;
    }
    ::SetLastError(ERROR_SUCCESS);
    if (ImageList_Remove(handle_, index) != FALSE) return true;
    if (::GetLastError() == ERROR_SUCCESS) ::SetLastError(ERROR_FUNCTION_FAILED);
    return false;
}
bool ImageList::RemoveAll() noexcept {
    if (handle_ == nullptr) {
        ::SetLastError(ERROR_INVALID_STATE);
        return false;
    }
    ::SetLastError(ERROR_SUCCESS);
    if (ImageList_Remove(handle_, -1) != FALSE) return true;
    if (::GetLastError() == ERROR_SUCCESS) ::SetLastError(ERROR_FUNCTION_FAILED);
    return false;
}
bool ImageList::SetOverlayImage(int image_index, int overlay_index) noexcept {
    if (handle_ == nullptr || image_index < 0 || image_index >= GetCount() || overlay_index < 1 ||
        overlay_index > 15) {
        ::SetLastError(ERROR_INVALID_PARAMETER);
        return false;
    }
    ::SetLastError(ERROR_SUCCESS);
    if (ImageList_SetOverlayImage(handle_, image_index, overlay_index) != FALSE) return true;
    if (::GetLastError() == ERROR_SUCCESS) ::SetLastError(ERROR_FUNCTION_FAILED);
    return false;
}
bool ImageList::SetBackgroundColor(COLORREF color) noexcept {
    if (handle_ == nullptr) {
        ::SetLastError(ERROR_INVALID_STATE);
        return false;
    }
    ImageList_SetBkColor(handle_, color);
    return true;
}
COLORREF ImageList::GetBackgroundColor() const noexcept {
    return handle_ != nullptr ? ImageList_GetBkColor(handle_) : CLR_NONE;
}
bool ImageList::GetIconSize(SIZE& size) const noexcept {
    size = {};
    if (handle_ == nullptr) {
        ::SetLastError(ERROR_INVALID_STATE);
        return false;
    }
    int width = 0;
    int height = 0;
    if (ImageList_GetIconSize(handle_, &width, &height) == FALSE) return false;
    size.cx = width;
    size.cy = height;
    return true;
}
int ImageList::GetCount() const noexcept { return handle_ != nullptr ? ImageList_GetImageCount(handle_) : 0; }
void ImageList::Reset() noexcept { if (handle_ != nullptr) ImageList_Destroy(handle_); handle_ = nullptr; }

Tooltip::~Tooltip() noexcept { Destroy(); }
Tooltip::Tooltip(Tooltip&& other) noexcept : window_(std::exchange(other.window_, nullptr)), owner_(std::exchange(other.owner_, nullptr)), texts_(std::move(other.texts_)) {}
Tooltip& Tooltip::operator=(Tooltip&& other) noexcept { if (this != &other) { Destroy(); window_ = std::exchange(other.window_, nullptr); owner_ = std::exchange(other.owner_, nullptr); texts_ = std::move(other.texts_); } return *this; }
bool Tooltip::Create(HWND owner, TooltipOptions options) {
    Destroy();
    if (owner == nullptr || ::IsWindow(owner) == FALSE || options.max_width < 0) {
        ::SetLastError(ERROR_INVALID_PARAMETER);
        return false;
    }
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_WIN95_CLASSES};
    if (::InitCommonControlsEx(&controls) == FALSE) return false;
    DWORD style = WS_POPUP;
    if (options.always_tip) style |= TTS_ALWAYSTIP;
    if (options.no_prefix) style |= TTS_NOPREFIX;
    if (options.balloon || options.close_button) style |= TTS_BALLOON;
    if (options.close_button) style |= TTS_CLOSE;
    window_ = ::CreateWindowExW(
        WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr, style, CW_USEDEFAULT, CW_USEDEFAULT,
        CW_USEDEFAULT, CW_USEDEFAULT, owner, nullptr,
        reinterpret_cast<HINSTANCE>(::GetWindowLongPtrW(owner, GWLP_HINSTANCE)), nullptr);
    if (window_ == nullptr) return false;
    owner_ = owner;
    ::SetWindowPos(window_, HWND_TOPMOST, 0, 0, 0, 0,
                   SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    if (options.max_width > 0) static_cast<void>(SetMaxWidth(options.max_width));
    return true;
}
bool Tooltip::AddTool(HWND tool, std::wstring_view text, TooltipToolOptions options) {
    if (!IsWindow() || tool == nullptr || ::IsWindow(tool) == FALSE ||
        (tool != owner_ && ::IsChild(owner_, tool) == FALSE) || texts_.contains(tool)) {
        ::SetLastError(ERROR_INVALID_PARAMETER);
        return false;
    }
    auto stored = std::make_unique<std::wstring>(text);
    TOOLINFOW info{};
    info.cbSize = sizeof(info);
    info.uFlags = TTF_IDISHWND;
    if (options.subclass) info.uFlags |= TTF_SUBCLASS;
    if (options.center_tip) info.uFlags |= TTF_CENTERTIP;
    if (options.transparent) info.uFlags |= TTF_TRANSPARENT;
    info.hwnd = owner_;
    info.uId = reinterpret_cast<UINT_PTR>(tool);
    info.lpszText = stored->data();
    if (::SendMessageW(window_, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&info)) == FALSE) {
        ::SetLastError(ERROR_FUNCTION_FAILED);
        return false;
    }
    texts_.emplace(tool, std::move(stored));
    return true;
}
bool Tooltip::UpdateTool(HWND tool, std::wstring_view text) {
    const auto found = texts_.find(tool);
    if (!IsWindow() || found == texts_.end()) {
        ::SetLastError(ERROR_NOT_FOUND);
        return false;
    }
    auto replacement = std::make_unique<std::wstring>(text);
    TOOLINFOW info{};
    info.cbSize = sizeof(info);
    info.hwnd = owner_;
    info.uId = reinterpret_cast<UINT_PTR>(tool);
    info.lpszText = replacement->data();
    ::SendMessageW(window_, TTM_UPDATETIPTEXTW, 0, reinterpret_cast<LPARAM>(&info));
    found->second = std::move(replacement);
    return true;
}
bool Tooltip::RemoveTool(HWND tool) noexcept {
    const auto found = texts_.find(tool);
    if (!IsWindow() || found == texts_.end()) {
        ::SetLastError(ERROR_NOT_FOUND);
        return false;
    }
    TOOLINFOW info{};
    info.cbSize = sizeof(info);
    info.hwnd = owner_;
    info.uId = reinterpret_cast<UINT_PTR>(tool);
    ::SendMessageW(window_, TTM_DELTOOLW, 0, reinterpret_cast<LPARAM>(&info));
    texts_.erase(found);
    return true;
}
bool Tooltip::HasTool(HWND tool) const noexcept { return texts_.contains(tool); }
int Tooltip::GetToolCount() const noexcept {
    return IsWindow() ? static_cast<int>(::SendMessageW(window_, TTM_GETTOOLCOUNT, 0, 0)) : 0;
}
bool Tooltip::SetActive(bool active) noexcept {
    if (!IsWindow()) {
        ::SetLastError(ERROR_INVALID_STATE);
        return false;
    }
    ::SendMessageW(window_, TTM_ACTIVATE, active ? TRUE : FALSE, 0);
    return true;
}
bool Tooltip::SetTitle(UINT icon, std::wstring_view title) {
    if (!IsWindow() || title.size() > 99 || icon > TTI_ERROR_LARGE) {
        ::SetLastError(title.size() > 99 ? ERROR_BUFFER_OVERFLOW : ERROR_INVALID_PARAMETER);
        return false;
    }
    const std::wstring owned{title};
    return ::SendMessageW(window_, TTM_SETTITLEW, static_cast<WPARAM>(icon),
                          reinterpret_cast<LPARAM>(owned.c_str())) != FALSE;
}
bool Tooltip::SetMaxWidth(int width) noexcept {
    if (!IsWindow() || width <= 0) {
        ::SetLastError(!IsWindow() ? ERROR_INVALID_STATE : ERROR_INVALID_PARAMETER);
        return false;
    }
    ::SendMessageW(window_, TTM_SETMAXTIPWIDTH, 0, width);
    return true;
}
bool Tooltip::SetDelayTime(UINT duration, int milliseconds) noexcept {
    if (!IsWindow() || milliseconds < 0 || milliseconds > USHRT_MAX ||
        (duration != TTDT_AUTOMATIC && duration != TTDT_AUTOPOP && duration != TTDT_INITIAL &&
         duration != TTDT_RESHOW)) {
        ::SetLastError(!IsWindow() ? ERROR_INVALID_STATE : ERROR_INVALID_PARAMETER);
        return false;
    }
    ::SendMessageW(window_, TTM_SETDELAYTIME, duration, MAKELPARAM(milliseconds, 0));
    return true;
}
bool Tooltip::RelayEvent(const MSG& message) noexcept {
    if (!IsWindow()) {
        ::SetLastError(ERROR_INVALID_STATE);
        return false;
    }
    MSG copy = message;
    ::SendMessageW(window_, TTM_RELAYEVENT, 0, reinterpret_cast<LPARAM>(&copy));
    return true;
}
void Tooltip::Pop() noexcept { if (IsWindow()) ::SendMessageW(window_, TTM_POP, 0, 0); }
void Tooltip::Update() noexcept { if (IsWindow()) ::SendMessageW(window_, TTM_UPDATE, 0, 0); }
bool Tooltip::IsWindow() const noexcept {
    return window_ != nullptr && ::IsWindow(window_) != FALSE;
}
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

}  // namespace mwfl
