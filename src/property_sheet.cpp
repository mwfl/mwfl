#include <mwfl/property_sheet.h>

#include <windows.h>
#include <commctrl.h>
#include <prsht.h>

#include <mwfl/message_pump.h>

#include <algorithm>
#include <cassert>
#include <stdexcept>
#include <utility>

namespace mwfl {
namespace {

constexpr UINT_PTR kSheetSubclassId = 1;
constexpr UINT kCloseModelessSheet = WM_APP + 0x51A;

struct BlankPageTemplate {
    DLGTEMPLATE dialog{};
    WORD menu = 0;
    WORD window_class = 0;
    WORD title = 0;

    BlankPageTemplate() noexcept {
        dialog.style = WS_CHILD | WS_VISIBLE | DS_CONTROL;
        dialog.dwExtendedStyle = WS_EX_CONTROLPARENT;
        dialog.cdit = 0;
        dialog.x = 0;
        dialog.y = 0;
        dialog.cx = 280;
        dialog.cy = 190;
    }
};

thread_local void* creating_sheet = nullptr;

}  // namespace

PropertyPageEntry* PropertySheetModel::FindMutable(PropertyPageId id) noexcept {
    const auto found = std::find_if(pages_.begin(), pages_.end(),
                                    [id](const PropertyPageEntry& page) { return page.id == id; });
    return found == pages_.end() ? nullptr : &*found;
}

bool PropertySheetModel::Add(PropertyPageEntry page) {
    if (page.id.value == 0 || Find(page.id) != nullptr) return false;
    const PropertyPageId id = page.id;
    pages_.push_back(std::move(page));
    if (!selected_.has_value()) selected_ = id;
    return true;
}

bool PropertySheetModel::Remove(PropertyPageId id) noexcept {
    const auto found = std::find_if(pages_.begin(), pages_.end(),
                                    [id](const PropertyPageEntry& page) { return page.id == id; });
    if (found == pages_.end()) return false;
    const std::size_t index = static_cast<std::size_t>(found - pages_.begin());
    const bool selected = selected_ == id;
    pages_.erase(found);
    if (pages_.empty()) {
        selected_.reset();
    } else if (selected) {
        selected_ = pages_[(std::min)(index, pages_.size() - 1)].id;
    }
    return true;
}

bool PropertySheetModel::Select(PropertyPageId id) noexcept {
    if (Find(id) == nullptr) return false;
    selected_ = id;
    return true;
}

bool PropertySheetModel::SetDirty(PropertyPageId id, bool dirty) noexcept {
    PropertyPageEntry* page = FindMutable(id);
    if (page == nullptr) return false;
    page->dirty = dirty;
    return true;
}

bool PropertySheetModel::Apply(PropertyPageId id, PropertyPageApplyOutcome outcome) noexcept {
    PropertyPageEntry* page = FindMutable(id);
    if (page == nullptr) return false;
    if (outcome == PropertyPageApplyOutcome::applied) page->dirty = false;
    return true;
}

void PropertySheetModel::Reset() noexcept {
    for (PropertyPageEntry& page : pages_) page.dirty = false;
}

std::span<const PropertyPageEntry> PropertySheetModel::GetPages() const noexcept {
    return pages_;
}

std::optional<PropertyPageId> PropertySheetModel::GetSelectedId() const noexcept {
    return selected_;
}

const PropertyPageEntry* PropertySheetModel::Find(PropertyPageId id) const noexcept {
    const auto found = std::find_if(pages_.begin(), pages_.end(),
                                    [id](const PropertyPageEntry& page) { return page.id == id; });
    return found == pages_.end() ? nullptr : &*found;
}

bool PropertySheetModel::AnyDirty() const noexcept {
    return std::any_of(pages_.begin(), pages_.end(),
                       [](const PropertyPageEntry& page) { return page.dirty; });
}

struct PropertyPage::State {
    explicit State(PropertyPageOptions input)
        : id(input.id), title(std::move(input.title)), callbacks(std::move(input.callbacks)) {}

    PropertyPageId id{};
    std::wstring title;
    PropertyPageCallbacks callbacks;
    BlankPageTemplate page_template;
    std::unique_ptr<LayoutHost> layout;
    HWND window = nullptr;         // Borrowed while the native page exists.
    void* active_sheet = nullptr;  // PropertySheetDialog::State, owned by the sheet.
    void (*dirty_changed)(void*, PropertyPageId, bool, HWND) noexcept = nullptr;
    void* dirty_changed_context = nullptr;
    bool dirty = false;
};

struct PropertySheetDialog::State {
    struct AnchoredControl {
        HWND window = nullptr;
        RECT bounds{};
    };

    class Filter final : public MessageFilter {
    public:
        explicit Filter(State& input) noexcept : state(input) {}
        bool PreTranslateMessage(MSG& message) override {
            if (state.window == nullptr || ::IsWindow(state.window) == FALSE)
                return false;
            const BOOL handled = PropSheet_IsDialogMessage(state.window, &message);
            if (PropSheet_GetCurrentPageHwnd(state.window) == nullptr) {
                state.result.native_result = PropSheet_GetResult(state.window);
                ::DestroyWindow(state.window);
            }
            return handled != FALSE;
        }

    private:
        State& state;
    };

    bool Prepare(const PropertySheetOptions& input, std::span<PropertyPage> input_pages) {
        if (input_pages.empty()) {
            ::SetLastError(ERROR_INVALID_PARAMETER);
            return false;
        }
        options = input;
        pages.clear();
        descriptors.clear();
        model = PropertySheetModel{};
        pages.reserve(input_pages.size());
        descriptors.reserve(input_pages.size());
        for (PropertyPage& page : input_pages) {
            if (!page.state_ || page.state_->id.value == 0 || page.state_->title.empty() ||
                page.state_->active_sheet != nullptr ||
                !model.Add({page.state_->id, page.state_->title, page.state_->dirty})) {
                ::SetLastError(ERROR_INVALID_PARAMETER);
                ClearPages();
                return false;
            }
            pages.push_back(page.state_);
            page.state_->active_sheet = this;
        }
        if (options.start_page.has_value() && model.Find(*options.start_page) == nullptr) {
            ::SetLastError(ERROR_INVALID_PARAMETER);
            ClearPages();
            return false;
        }
        for (const std::shared_ptr<PropertyPage::State>& page : pages) {
            PROPSHEETPAGEW descriptor{};
            descriptor.dwSize = sizeof(descriptor);
            descriptor.dwFlags = PSP_USETITLE | PSP_DLGINDIRECT;
            descriptor.hInstance = ::GetModuleHandleW(nullptr);
            descriptor.pResource = &page->page_template.dialog;
            descriptor.pszTitle = page->title.c_str();
            descriptor.pfnDlgProc = PropertySheetDialog::PageProcedure;
            descriptor.lParam = reinterpret_cast<LPARAM>(page.get());
            descriptors.push_back(descriptor);
        }
        owner_thread = ::GetCurrentThreadId();
        filter = std::make_unique<Filter>(*this);
        for (const std::shared_ptr<PropertyPage::State>& page : pages) {
            page->dirty_changed_context = this;
            page->dirty_changed = [](void* context, PropertyPageId id, bool dirty,
                                     HWND page_window) noexcept {
                auto& sheet = *static_cast<State*>(context);
                static_cast<void>(sheet.model.SetDirty(id, dirty));
                const HWND parent = page_window == nullptr ? nullptr : ::GetParent(page_window);
                if (parent == nullptr) return;
                if (dirty) {
                    PropSheet_Changed(parent, page_window);
                } else {
                    PropSheet_UnChanged(parent, page_window);
                }
            };
        }
        result = {};
        result.status = PropertySheetStatus::cancelled;
        return true;
    }

    void ClearPages() noexcept {
        for (const std::shared_ptr<PropertyPage::State>& page : pages) {
            if (page->active_sheet == this) page->active_sheet = nullptr;
            page->dirty_changed = nullptr;
            page->dirty_changed_context = nullptr;
            page->window = nullptr;
        }
        pages.clear();
        descriptors.clear();
    }

    void RegisterFilter() noexcept {
        if (filter_registered) return;
        MessageLoop* loop = MessageLoop::Current();
        if (loop == nullptr) return;
        filter_registered = loop->AddFilter(filter.get());
    }

    void UnregisterFilter() noexcept {
        if (!filter_registered) return;
        if (MessageLoop* loop = MessageLoop::Current(); loop != nullptr && filter) {
            loop->RemoveFilter(filter.get());
        }
        filter_registered = false;
    }

    void RecordException() noexcept {
        if (!result.callback_exception) result.callback_exception = std::current_exception();
        result.status = PropertySheetStatus::failed;
        result.error = ERROR_UNHANDLED_EXCEPTION;
    }

    static RECT BoundsInClient(HWND parent, HWND child) noexcept {
        RECT bounds{};
        if (child != nullptr && ::GetWindowRect(child, &bounds) != FALSE) {
            ::MapWindowPoints(nullptr, parent, reinterpret_cast<POINT*>(&bounds), 2);
        }
        return bounds;
    }

    void CaptureLayout(HWND sheet) noexcept {
        ::GetClientRect(sheet, &initial_client);
        tab = {PropSheet_GetTabControl(sheet), {}};
        tab.bounds = BoundsInClient(sheet, tab.window);
        const HWND current_page = PropSheet_GetCurrentPageHwnd(sheet);
        page_bounds = BoundsInClient(sheet, current_page);
        anchored_controls.clear();
        // comctl32's Apply button identifier; WTL's atlres.h formerly
        // provided this value as ID_APPLY_NOW.
        constexpr int kApplyButtonId = 0x3021;
        for (const int id : {IDOK, IDCANCEL, kApplyButtonId, IDHELP}) {
            const HWND control = ::GetDlgItem(sheet, id);
            if (control != nullptr) {
                anchored_controls.push_back({control, BoundsInClient(sheet, control)});
            }
        }
    }

    void ArrangeSheet(HWND sheet) noexcept {
        if (!options.resizable || initial_client.right == 0 || initial_client.bottom == 0) return;
        if (page_bounds.right == 0 || page_bounds.bottom == 0) {
            page_bounds = BoundsInClient(sheet, PropSheet_GetCurrentPageHwnd(sheet));
            if (page_bounds.right == 0 || page_bounds.bottom == 0) return;
        }
        RECT client{};
        if (::GetClientRect(sheet, &client) == FALSE) return;
        const int dx = client.right - initial_client.right;
        const int dy = client.bottom - initial_client.bottom;
        if (tab.window != nullptr) {
            ::SetWindowPos(tab.window, nullptr, tab.bounds.left, tab.bounds.top,
                           tab.bounds.right - tab.bounds.left + dx,
                           tab.bounds.bottom - tab.bounds.top + dy, SWP_NOACTIVATE | SWP_NOZORDER);
        }
        for (const std::shared_ptr<PropertyPage::State>& page : pages) {
            if (page->window != nullptr) {
                ::SetWindowPos(page->window, nullptr, page_bounds.left, page_bounds.top,
                               page_bounds.right - page_bounds.left + dx,
                               page_bounds.bottom - page_bounds.top + dy,
                               SWP_NOACTIVATE | SWP_NOZORDER);
            }
        }
        for (const AnchoredControl& control : anchored_controls) {
            ::SetWindowPos(control.window, nullptr, control.bounds.left + dx,
                           control.bounds.top + dy, 0, 0,
                           SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER);
        }
    }

    PROPSHEETHEADERW MakeHeader(bool is_modeless) noexcept {
        PROPSHEETHEADERW header{};
        header.dwSize = sizeof(header);
        header.dwFlags = PSH_PROPSHEETPAGE | PSH_USECALLBACK;
        if (is_modeless) header.dwFlags |= PSH_MODELESS;
        if (options.resizable) header.dwFlags |= PSH_RESIZABLE;
        if (!options.show_apply) header.dwFlags |= PSH_NOAPPLYNOW;
        header.hwndParent = options.owner;
        header.hInstance = ::GetModuleHandleW(nullptr);
        header.pszCaption = options.title.c_str();
        header.nPages = static_cast<UINT>(descriptors.size());
        header.ppsp = descriptors.data();
        header.pfnCallback = PropertySheetDialog::SheetCallback;
        header.nStartPage = 0;
        if (options.start_page.has_value()) {
            for (std::size_t index = 0; index < pages.size(); ++index) {
                if (pages[index]->id == *options.start_page) {
                    header.nStartPage = static_cast<UINT>(index);
                    break;
                }
            }
        }
        return header;
    }

    PropertySheetOptions options;
    std::vector<std::shared_ptr<PropertyPage::State>> pages;
    std::vector<PROPSHEETPAGEW> descriptors;
    PropertySheetModel model;
    PropertySheetResult result;
    HWND window =
        nullptr;  // Owned by PropertySheetDialog for modeless; native modal call otherwise.
    DWORD owner_thread = 0;
    std::unique_ptr<Filter> filter;
    std::shared_ptr<State> keep_alive;
    bool filter_registered = false;
    bool modeless = false;
    int minimum_width = 0;
    int minimum_height = 0;
    RECT initial_client{};
    RECT page_bounds{};
    AnchoredControl tab{};
    std::vector<AnchoredControl> anchored_controls;
};

PropertyPage::PropertyPage(PropertyPageOptions options)
    : state_(std::make_shared<State>(std::move(options))) {
    if (state_->id.value == 0 || state_->title.empty()) {
        throw std::invalid_argument("property pages require a nonzero ID and title");
    }
}

PropertyPage::~PropertyPage() noexcept = default;
PropertyPage::PropertyPage(PropertyPage&&) noexcept = default;
PropertyPage& PropertyPage::operator=(PropertyPage&&) noexcept = default;

PropertyPageId PropertyPage::GetId() const noexcept {
    return state_ ? state_->id : PropertyPageId{};
}

std::wstring_view PropertyPage::GetTitle() const noexcept {
    return state_ ? std::wstring_view(state_->title) : std::wstring_view{};
}

HWND PropertyPage::GetHwnd() const noexcept {
    return state_ && state_->window != nullptr && ::IsWindow(state_->window) != FALSE
               ? state_->window
               : nullptr;
}

bool PropertyPage::IsDirty() const noexcept {
    return state_ && state_->dirty;
}

bool PropertyPage::SetDirty(bool dirty) noexcept {
    if (!state_) return false;
    state_->dirty = dirty;
    if (state_->dirty_changed) {
        state_->dirty_changed(state_->dirty_changed_context, state_->id, dirty, GetHwnd());
    }
    return true;
}

bool PropertyPage::SetLayout(LayoutNode root) {
    if (!state_) return false;
    state_->layout = std::make_unique<LayoutHost>(std::move(root));
    const HWND page = GetHwnd();
    return page == nullptr || state_->layout->Arrange(page);
}

void PropertyPage::ClearLayout() noexcept {
    if (state_) state_->layout.reset();
}

PropertySheetDialog::~PropertySheetDialog() noexcept {
    Close();
}

PropertySheetDialog::PropertySheetDialog(PropertySheetDialog&& other) noexcept
    : state_(std::move(other.state_)) {}

PropertySheetDialog& PropertySheetDialog::operator=(PropertySheetDialog&& other) noexcept {
    if (this == &other) return *this;
    Close();
    state_ = std::move(other.state_);
    return *this;
}

PropertySheetResult PropertySheetDialog::ShowModal(const PropertySheetOptions& options,
                                                   std::span<PropertyPage> pages) noexcept {
    auto state = std::make_shared<State>();
    try {
        if (!state->Prepare(options, pages)) {
            PropertySheetResult failed;
            failed.error = ::GetLastError();
            return failed;
        }
        PROPSHEETHEADERW header = state->MakeHeader(false);
        state->modeless = false;
        void* previous = creating_sheet;
        creating_sheet = state.get();
        const INT_PTR native_result = ::PropertySheetW(&header);
        creating_sheet = previous;
        state->result.native_result = native_result;
        if (native_result == -1) {
            state->result.status = PropertySheetStatus::failed;
            state->result.error = ::GetLastError();
            if (state->result.error == ERROR_SUCCESS) {
                state->result.error = ERROR_FUNCTION_FAILED;
            }
        } else if (state->result.status != PropertySheetStatus::accepted &&
                   state->result.status != PropertySheetStatus::failed) {
            state->result.status = PropertySheetStatus::cancelled;
        }
        state->UnregisterFilter();
        state->ClearPages();
        return state->result;
    } catch (...) {
        state->RecordException();
        state->UnregisterFilter();
        state->ClearPages();
        return state->result;
    }
}

bool PropertySheetDialog::CreateModeless(const PropertySheetOptions& options,
                                         std::span<PropertyPage> pages) noexcept {
    Close();
    auto state = std::make_shared<State>();
    try {
        if (!state->Prepare(options, pages)) return false;
        PROPSHEETHEADERW header = state->MakeHeader(true);
        state->modeless = true;
        void* previous = creating_sheet;
        creating_sheet = state.get();
        const INT_PTR native_result = ::PropertySheetW(&header);
        creating_sheet = previous;
        const HWND window = reinterpret_cast<HWND>(native_result);
        if (window == nullptr || window == reinterpret_cast<HWND>(-1) ||
            ::IsWindow(window) == FALSE) {
            state->result.status = PropertySheetStatus::failed;
            state->result.native_result = native_result;
            state->result.error = ::GetLastError();
            if (state->result.error == ERROR_SUCCESS) {
                state->result.error = ERROR_FUNCTION_FAILED;
            }
            state->ClearPages();
            return false;
        }
        state->window = window;
        state->result.native_result = native_result;
        state->keep_alive = state;
        state_ = std::move(state);
        return true;
    } catch (...) {
        state->RecordException();
        state->ClearPages();
        ::SetLastError(ERROR_UNHANDLED_EXCEPTION);
        return false;
    }
}

HWND PropertySheetDialog::GetHwnd() const noexcept {
    return state_ && state_->window != nullptr && ::IsWindow(state_->window) != FALSE
               ? state_->window
               : nullptr;
}

bool PropertySheetDialog::IsWindow() const noexcept {
    return GetHwnd() != nullptr;
}

bool PropertySheetDialog::IsOwnerThread() const noexcept {
    return state_ && state_->owner_thread == ::GetCurrentThreadId();
}

PropertySheetResult PropertySheetDialog::GetResult() const noexcept {
    return state_ ? state_->result : PropertySheetResult{};
}

void PropertySheetDialog::Close() noexcept {
    if (!state_) return;
    const HWND window = GetHwnd();
    if (window != nullptr) {
        if (IsOwnerThread()) {
            ::SendMessageW(window, WM_CLOSE, 0, 0);
            if (::IsWindow(window) != FALSE) ::DestroyWindow(window);
        } else {
            ::PostMessageW(window, kCloseModelessSheet, 0, 0);
        }
    }
    if (state_->window == nullptr) {
        state_->UnregisterFilter();
        state_->ClearPages();
    }
    state_.reset();
}

INT_PTR CALLBACK PropertySheetDialog::PageProcedure(HWND page, UINT message, WPARAM wparam,
                                                    LPARAM lparam) noexcept {
    PropertyPage::State* state =
        reinterpret_cast<PropertyPage::State*>(::GetWindowLongPtrW(page, DWLP_USER));
    if (message == WM_INITDIALOG) {
        const auto* descriptor = reinterpret_cast<const PROPSHEETPAGEW*>(lparam);
        state = descriptor == nullptr ? nullptr
                                      : reinterpret_cast<PropertyPage::State*>(descriptor->lParam);
        ::SetWindowLongPtrW(page, DWLP_USER, reinterpret_cast<LONG_PTR>(state));
        if (state == nullptr) return FALSE;
        state->window = page;
        auto* sheet = static_cast<State*>(state->active_sheet);
        try {
            if (state->callbacks.initialize && !state->callbacks.initialize(page)) {
                if (sheet != nullptr) {
                    sheet->result.status = PropertySheetStatus::failed;
                    sheet->result.error = ERROR_FUNCTION_FAILED;
                }
                ::PostMessageW(::GetParent(page), PSM_PRESSBUTTON, PSBTN_CANCEL, 0);
                return FALSE;
            }
            if (state->layout && !state->layout->Arrange(page)) {
                if (sheet != nullptr) {
                    sheet->result.status = PropertySheetStatus::failed;
                    sheet->result.error = ::GetLastError();
                }
                ::PostMessageW(::GetParent(page), PSM_PRESSBUTTON, PSBTN_CANCEL, 0);
                return FALSE;
            }
            if (sheet != nullptr) sheet->ArrangeSheet(::GetParent(page));
            if (state->dirty) PropSheet_Changed(::GetParent(page), page);
        } catch (...) {
            if (sheet != nullptr) sheet->RecordException();
            ::PostMessageW(::GetParent(page), PSM_PRESSBUTTON, PSBTN_CANCEL, 0);
            return FALSE;
        }
        return TRUE;
    }
    if (state == nullptr) return FALSE;
    auto* sheet = static_cast<State*>(state->active_sheet);
    try {
        if (message == WM_COMMAND && state->callbacks.command) {
            return state->callbacks.command(page, LOWORD(wparam), HIWORD(wparam)) ? TRUE : FALSE;
        }
        if ((message == WM_SIZE || message == WM_DPICHANGED_AFTERPARENT) && state->layout) {
            static_cast<void>(state->layout->Arrange(page));
            return TRUE;
        }
        if (message == WM_NOTIFY) {
            const auto* header = reinterpret_cast<const NMHDR*>(lparam);
            if (header == nullptr) return FALSE;
            if (header->code == PSN_SETACTIVE) {
                if (sheet != nullptr) static_cast<void>(sheet->model.Select(state->id));
                return TRUE;
            }
            if (header->code == PSN_KILLACTIVE) {
                const PropertyPageValidation validation = state->callbacks.validate
                                                              ? state->callbacks.validate(page)
                                                              : PropertyPageValidation::valid;
                ::SetWindowLongPtrW(page, DWLP_MSGRESULT,
                                    validation == PropertyPageValidation::valid ? FALSE : TRUE);
                return TRUE;
            }
            if (header->code == PSN_APPLY) {
                const PropertyPageValidation validation = state->callbacks.validate
                                                              ? state->callbacks.validate(page)
                                                              : PropertyPageValidation::valid;
                if (validation == PropertyPageValidation::invalid) {
                    if (sheet != nullptr) {
                        static_cast<void>(sheet->model.Apply(
                            state->id, PropertyPageApplyOutcome::validation_failed));
                    }
                    ::SetWindowLongPtrW(page, DWLP_MSGRESULT, PSNRET_INVALID_NOCHANGEPAGE);
                    return TRUE;
                }
                if (state->callbacks.apply && !state->callbacks.apply(page)) {
                    if (sheet != nullptr) {
                        static_cast<void>(
                            sheet->model.Apply(state->id, PropertyPageApplyOutcome::failed));
                        sheet->result.status = PropertySheetStatus::failed;
                        sheet->result.error = ERROR_FUNCTION_FAILED;
                    }
                    ::SetWindowLongPtrW(page, DWLP_MSGRESULT, PSNRET_INVALID);
                    return TRUE;
                }
                state->dirty = false;
                if (sheet != nullptr) {
                    static_cast<void>(
                        sheet->model.Apply(state->id, PropertyPageApplyOutcome::applied));
                    const auto* notification = reinterpret_cast<const PSHNOTIFY*>(lparam);
                    if (notification->lParam != 0) {
                        sheet->result.status = PropertySheetStatus::accepted;
                    }
                }
                PropSheet_UnChanged(::GetParent(page), page);
                ::SetWindowLongPtrW(page, DWLP_MSGRESULT, PSNRET_NOERROR);
                return TRUE;
            }
            if (header->code == PSN_RESET) {
                if (state->callbacks.reset) state->callbacks.reset(page);
                state->dirty = false;
                if (sheet != nullptr) {
                    static_cast<void>(sheet->model.SetDirty(state->id, false));
                    if (sheet->result.status != PropertySheetStatus::failed) {
                        sheet->result.status = PropertySheetStatus::cancelled;
                    }
                }
                return TRUE;
            }
        }
        if (message == WM_DESTROY) {
            state->window = nullptr;
        }
    } catch (...) {
        if (sheet != nullptr) sheet->RecordException();
        if (message == WM_NOTIFY) {
            const auto* header = reinterpret_cast<const NMHDR*>(lparam);
            if (header != nullptr && header->code != PSN_RESET) {
                ::PostMessageW(::GetParent(page), PSM_PRESSBUTTON, PSBTN_CANCEL, 0);
            }
            ::SetWindowLongPtrW(page, DWLP_MSGRESULT, PSNRET_INVALID_NOCHANGEPAGE);
            return TRUE;
        }
    }
    return FALSE;
}

int CALLBACK PropertySheetDialog::SheetCallback(HWND sheet, UINT message, LPARAM) noexcept {
    if (message != PSCB_INITIALIZED || creating_sheet == nullptr) return 0;
    auto* state = static_cast<State*>(creating_sheet);
    state->window = sheet;
    state->owner_thread = ::GetCurrentThreadId();
    if (state->options.resizable) {
        RECT bounds{};
        ::GetWindowRect(sheet, &bounds);
        state->minimum_width = bounds.right - bounds.left;
        state->minimum_height = bounds.bottom - bounds.top;
        const LONG_PTR style = ::GetWindowLongPtrW(sheet, GWL_STYLE);
        ::SetWindowLongPtrW(sheet, GWL_STYLE, style | WS_THICKFRAME | WS_MAXIMIZEBOX);
        ::SetWindowPos(sheet, nullptr, 0, 0, 0, 0,
                       SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        state->CaptureLayout(sheet);
    }
    if (::SetWindowSubclass(sheet, SheetSubclass, kSheetSubclassId,
                            reinterpret_cast<DWORD_PTR>(state)) == FALSE) {
        state->result.status = PropertySheetStatus::failed;
        state->result.error = ::GetLastError();
        ::PostMessageW(sheet, PSM_PRESSBUTTON, PSBTN_CANCEL, 0);
        return 0;
    }
    if (state->modeless) state->RegisterFilter();
    return 0;
}

LRESULT CALLBACK PropertySheetDialog::SheetSubclass(HWND sheet, UINT message, WPARAM wparam,
                                                    LPARAM lparam, UINT_PTR subclass_id,
                                                    DWORD_PTR reference) noexcept {
    auto* state = reinterpret_cast<State*>(reference);
    if (message == kCloseModelessSheet && state != nullptr) {
        ::SendMessageW(sheet, WM_CLOSE, 0, 0);
        if (::IsWindow(sheet) != FALSE) ::DestroyWindow(sheet);
        return 0;
    }
    if (message == WM_GETMINMAXINFO && state != nullptr && state->options.resizable) {
        auto* limits = reinterpret_cast<MINMAXINFO*>(lparam);
        if (limits != nullptr) {
            limits->ptMinTrackSize.x = state->minimum_width;
            limits->ptMinTrackSize.y = state->minimum_height;
            return 0;
        }
    }
    if (message == WM_NCDESTROY && state != nullptr) {
        const std::shared_ptr<State> keep_alive = state->keep_alive;
        state->UnregisterFilter();
        state->window = nullptr;
        state->ClearPages();
        ::RemoveWindowSubclass(sheet, SheetSubclass, subclass_id);
        state->keep_alive.reset();
        return ::DefSubclassProc(sheet, message, wparam, lparam);
    }
    const LRESULT result = ::DefSubclassProc(sheet, message, wparam, lparam);
    if (state != nullptr && message == WM_SIZE) state->ArrangeSheet(sheet);
    if (state != nullptr && state->modeless && message == PSM_PRESSBUTTON &&
        state->window != nullptr && PropSheet_GetCurrentPageHwnd(sheet) == nullptr) {
        state->result.native_result = PropSheet_GetResult(sheet);
        ::DestroyWindow(sheet);
    }
    return result;
}

}  // namespace mwfl
