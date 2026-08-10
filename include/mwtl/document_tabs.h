#pragma once

#include <windows.h>

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

#include <mwtl/document_workspace.h>
#include <mwtl/navigation_controls.h>

namespace mwtl {

struct DocumentPageBinding {
    DocumentId document{};
    HWND page = nullptr;  // Borrowed direct child of the attached TabControl.
};

enum class DocumentTabStatus {
    success,
    invalid_argument,
    not_attached,
    not_found,
    stale_window,
    invalid_parent,
    wrong_thread_or_process,
    model_failure,
    native_failure,
    rollback_failure,
    projection_failure,
};

struct DocumentTabResult {
    DocumentTabStatus status = DocumentTabStatus::invalid_argument;
    std::optional<DocumentId> active;
    explicit operator bool() const noexcept {
        return status == DocumentTabStatus::success;
    }
};

// UI-thread adapter. It borrows the TabControl HWND and every page HWND; the
// application retains destruction ownership. Native tab item data contains
// only the stable integer DocumentId, never an application pointer.
class DocumentTabWorkspaceAdapter final {
public:
    DocumentTabStatus Attach(const TabControl& tabs) noexcept;
    void Detach() noexcept;
    DocumentTabStatus BindPage(DocumentId document, HWND page);
    DocumentTabStatus UnbindPage(DocumentId document) noexcept;
    DocumentTabResult Synchronize(const DocumentWorkspaceModel& workspace,
                                  bool focus_active = false);
    DocumentTabResult ActivateNativeSelection(DocumentWorkspaceModel& workspace);
    DocumentTabStatus ArrangePages() noexcept;

    HWND GetTabHwnd() const noexcept { return tabs_; }
    std::span<const DocumentPageBinding> GetPages() const noexcept { return pages_; }
    HWND FindPage(DocumentId document) const noexcept;

private:
    friend DocumentTabResult TransferDocumentWithPage(
        DocumentWorkspaceModel&, DocumentTabWorkspaceAdapter&,
        DocumentWorkspaceModel&, DocumentTabWorkspaceAdapter&, DocumentId,
        std::optional<std::size_t>);
    bool IsAttached() const noexcept;
    bool IsValidPage(HWND page) const noexcept;
    std::optional<std::size_t> FindPageIndex(DocumentId document) const noexcept;

    HWND tabs_ = nullptr;
    DWORD ui_thread_ = 0;
    DWORD process_ = 0;
    std::vector<DocumentPageBinding> pages_;
};

// Transfers logical metadata and one application-owned page HWND together.
// Destination adoption precedes source removal. A failed reparent or source
// commit restores destination metadata and the original HWND parent.
DocumentTabResult TransferDocumentWithPage(
    DocumentWorkspaceModel& source, DocumentTabWorkspaceAdapter& source_tabs,
    DocumentWorkspaceModel& destination,
    DocumentTabWorkspaceAdapter& destination_tabs, DocumentId document,
    std::optional<std::size_t> destination_index = std::nullopt);

}  // namespace mwtl
