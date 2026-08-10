#include <mwfl/document_tabs.h>

#include <commctrl.h>

#include <algorithm>
#include <ranges>
#include <utility>

namespace mwfl {

bool DocumentTabWorkspaceAdapter::IsAttached() const noexcept {
    if (!tabs_ || !::IsWindow(tabs_)) return false;
    DWORD process = 0;
    return ::GetWindowThreadProcessId(tabs_, &process) == ui_thread_ &&
           process == process_;
}

bool DocumentTabWorkspaceAdapter::IsValidPage(HWND page) const noexcept {
    if (!IsAttached() || !page || !::IsWindow(page) ||
        ::GetParent(page) != ::GetParent(tabs_))
        return false;
    DWORD process = 0;
    return ::GetWindowThreadProcessId(page, &process) == ui_thread_ &&
           process == process_;
}

DocumentTabStatus DocumentTabWorkspaceAdapter::Attach(const TabControl& tabs) noexcept {
    const HWND hwnd = tabs.GetHwnd();
    if (!hwnd || !::IsWindow(hwnd)) return DocumentTabStatus::invalid_argument;
    DWORD process = 0;
    const DWORD thread = ::GetWindowThreadProcessId(hwnd, &process);
    if (!thread || process != ::GetCurrentProcessId() ||
        thread != ::GetCurrentThreadId())
        return DocumentTabStatus::wrong_thread_or_process;
    tabs_ = hwnd;
    ui_thread_ = thread;
    process_ = process;
    pages_.clear();
    return DocumentTabStatus::success;
}

void DocumentTabWorkspaceAdapter::Detach() noexcept {
    pages_.clear();
    tabs_ = nullptr;
    ui_thread_ = 0;
    process_ = 0;
}

std::optional<std::size_t> DocumentTabWorkspaceAdapter::FindPageIndex(
    DocumentId document) const noexcept {
    const auto found = std::ranges::find(pages_, document,
                                         &DocumentPageBinding::document);
    if (found == pages_.end()) return std::nullopt;
    return static_cast<std::size_t>(found - pages_.begin());
}

HWND DocumentTabWorkspaceAdapter::FindPage(DocumentId document) const noexcept {
    const auto index = FindPageIndex(document);
    return index ? pages_[*index].page : nullptr;
}

DocumentTabStatus DocumentTabWorkspaceAdapter::BindPage(DocumentId document,
                                                         HWND page) {
    if (!document || !page) return DocumentTabStatus::invalid_argument;
    if (!IsAttached()) return DocumentTabStatus::not_attached;
    if (!::IsWindow(page)) return DocumentTabStatus::stale_window;
    if (::GetParent(page) != ::GetParent(tabs_))
        return DocumentTabStatus::invalid_parent;
    DWORD process = 0;
    if (::GetWindowThreadProcessId(page, &process) != ui_thread_ ||
        process != process_) return DocumentTabStatus::wrong_thread_or_process;
    if (FindPageIndex(document) || std::ranges::any_of(pages_, [=](const auto& item) {
            return item.page == page;
        })) return DocumentTabStatus::invalid_argument;
    pages_.push_back({document, page});
    return DocumentTabStatus::success;
}

DocumentTabStatus DocumentTabWorkspaceAdapter::UnbindPage(
    DocumentId document) noexcept {
    const auto index = FindPageIndex(document);
    if (!index) return DocumentTabStatus::not_found;
    pages_.erase(pages_.begin() + static_cast<std::ptrdiff_t>(*index));
    return DocumentTabStatus::success;
}

DocumentTabStatus DocumentTabWorkspaceAdapter::ArrangePages() noexcept {
    if (!IsAttached()) return DocumentTabStatus::not_attached;
    RECT tab_bounds{};
    if (!::GetWindowRect(tabs_, &tab_bounds) ||
        ::MapWindowPoints(nullptr, ::GetParent(tabs_),
                          reinterpret_cast<POINT*>(&tab_bounds), 2) == 0)
        return DocumentTabStatus::native_failure;
    RECT bounds{0, 0, tab_bounds.right - tab_bounds.left,
                tab_bounds.bottom - tab_bounds.top};
    TabCtrl_AdjustRect(tabs_, FALSE, &bounds);
    ::OffsetRect(&bounds, tab_bounds.left, tab_bounds.top);
    for (const auto& item : pages_) {
        if (!IsValidPage(item.page)) return DocumentTabStatus::stale_window;
        if (!::SetWindowPos(item.page, nullptr, bounds.left, bounds.top,
                            (std::max)(0L, bounds.right - bounds.left),
                            (std::max)(0L, bounds.bottom - bounds.top),
                            SWP_NOACTIVATE | SWP_NOZORDER))
            return DocumentTabStatus::native_failure;
    }
    return DocumentTabStatus::success;
}

DocumentTabResult DocumentTabWorkspaceAdapter::Synchronize(
    const DocumentWorkspaceModel& workspace, bool focus_active) {
    if (!IsAttached()) return {DocumentTabStatus::not_attached, {}};
    if (pages_.size() != workspace.GetCount())
        return {DocumentTabStatus::invalid_argument, workspace.GetActiveId()};
    TabWorkspaceModel projection;
    for (const auto& document : workspace.GetDocuments()) {
        const HWND page = FindPage(document.id);
        if (!IsValidPage(page))
            return {page ? DocumentTabStatus::stale_window
                         : DocumentTabStatus::not_found,
                    workspace.GetActiveId()};
        auto title = document.title;
        if (document.dirty) title += L" *";
        if (!projection.Add({TabId{document.id.value}, std::move(title),
                             document.dirty, true}))
            return {DocumentTabStatus::model_failure, workspace.GetActiveId()};
    }
    if (const auto active = workspace.GetActiveId(); active &&
        !projection.Select(TabId{active->value}))
        return {DocumentTabStatus::model_failure, active};
    // TabControl is an owning wrapper, so use native messages directly here to
    // avoid transferring HWND lifetime into a temporary wrapper.
    if (TabCtrl_DeleteAllItems(tabs_) == FALSE)
        return {DocumentTabStatus::projection_failure, workspace.GetActiveId()};
    for (const auto& tab : projection.GetTabs()) {
        TCITEMW item{};
        item.mask = TCIF_TEXT | TCIF_PARAM;
        item.pszText = const_cast<wchar_t*>(tab.title.c_str());
        item.lParam = static_cast<LPARAM>(tab.id.value);
        if (TabCtrl_InsertItem(tabs_, TabCtrl_GetItemCount(tabs_), &item) < 0)
            return {DocumentTabStatus::projection_failure, workspace.GetActiveId()};
    }
    if (const auto selected = projection.GetSelectedId()) {
        const auto index = projection.FindIndex(*selected);
        if (!index || TabCtrl_SetCurSel(tabs_, static_cast<int>(*index)) < 0)
            return {DocumentTabStatus::projection_failure, workspace.GetActiveId()};
    }
    if (ArrangePages() != DocumentTabStatus::success)
        return {DocumentTabStatus::native_failure, workspace.GetActiveId()};
    const auto active = workspace.GetActiveId();
    for (const auto& item : pages_)
        ::ShowWindow(item.page, active == item.document ? SW_SHOW : SW_HIDE);
    if (focus_active && active) {
        const HWND page = FindPage(*active);
        if (page) ::SetFocus(page);
    }
    return {DocumentTabStatus::success, active};
}

DocumentTabResult DocumentTabWorkspaceAdapter::ActivateNativeSelection(
    DocumentWorkspaceModel& workspace) {
    if (!IsAttached()) return {DocumentTabStatus::not_attached, {}};
    const int selected = TabCtrl_GetCurSel(tabs_);
    if (selected < 0) return {DocumentTabStatus::not_found, workspace.GetActiveId()};
    TCITEMW item{};
    item.mask = TCIF_PARAM;
    if (TabCtrl_GetItem(tabs_, selected, &item) == FALSE || item.lParam == 0)
        return {DocumentTabStatus::native_failure, workspace.GetActiveId()};
    const DocumentId id{static_cast<std::uint64_t>(item.lParam)};
    if (!workspace.Activate(id))
        return {DocumentTabStatus::model_failure, workspace.GetActiveId()};
    return Synchronize(workspace, true);
}

DocumentTabResult TransferDocumentWithPage(
    DocumentWorkspaceModel& source, DocumentTabWorkspaceAdapter& source_tabs,
    DocumentWorkspaceModel& destination,
    DocumentTabWorkspaceAdapter& destination_tabs, DocumentId document,
    std::optional<std::size_t> destination_index) {
    if (&source == &destination || &source_tabs == &destination_tabs || !document ||
        !source_tabs.IsAttached() || !destination_tabs.IsAttached())
        return {DocumentTabStatus::invalid_argument, source.GetActiveId()};
    const auto page_index = source_tabs.FindPageIndex(document);
    if (!page_index || destination_tabs.FindPageIndex(document))
        return {DocumentTabStatus::not_found, source.GetActiveId()};
    const HWND page = source_tabs.pages_[*page_index].page;
    if (!source_tabs.IsValidPage(page))
        return {DocumentTabStatus::stale_window, source.GetActiveId()};
    destination_tabs.pages_.reserve(destination_tabs.pages_.size() + 1);
    auto plan = source.PrepareTransfer(document, destination.GetId(), destination_index);
    if (!plan) return {DocumentTabStatus::model_failure, source.GetActiveId()};
    if (!destination.AcceptTransfer(*plan))
        return {DocumentTabStatus::model_failure, source.GetActiveId()};
    ::SetLastError(ERROR_SUCCESS);
    const HWND source_parent = ::GetParent(source_tabs.tabs_);
    const HWND destination_parent = ::GetParent(destination_tabs.tabs_);
    if (::SetParent(page, destination_parent) != source_parent) {
        const auto error = ::GetLastError();
        const bool model_restored = static_cast<bool>(destination.RollbackTransfer(*plan));
        ::SetLastError(error);
        return {model_restored ? DocumentTabStatus::native_failure
                               : DocumentTabStatus::rollback_failure,
                source.GetActiveId()};
    }
    if (!source.CommitTransfer(*plan)) {
        const bool parent_restored = ::SetParent(page, source_parent) ==
                                     destination_parent;
        const bool model_restored = static_cast<bool>(destination.RollbackTransfer(*plan));
        return {parent_restored && model_restored ? DocumentTabStatus::model_failure
                                                  : DocumentTabStatus::rollback_failure,
                source.GetActiveId()};
    }
    destination_tabs.pages_.push_back({document, page});
    source_tabs.pages_.erase(source_tabs.pages_.begin() +
                             static_cast<std::ptrdiff_t>(*page_index));
    if (plan->was_active) destination.Activate(document);
    const auto source_projection = source_tabs.Synchronize(source);
    const auto destination_projection = destination_tabs.Synchronize(destination, plan->was_active);
    if (!source_projection || !destination_projection)
        return {DocumentTabStatus::projection_failure, destination.GetActiveId()};
    return {DocumentTabStatus::success, destination.GetActiveId()};
}

}  // namespace mwfl
